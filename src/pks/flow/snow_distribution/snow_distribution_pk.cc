/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */

/* -----------------------------------------------------------------------------
This is the overland flow component of ATS.
License: BSD
Author: Ethan Coon (ecoon@lanl.gov)
----------------------------------------------------------------------------- */

#include "EpetraExt_MultiVectorOut.h"
#include "Epetra_MultiVector.h"

#include "Mesh.hh"
#include "Point.hh"

#include "FunctionFactory.hh"
#include "composite_vector_function.hh"
#include "composite_vector_function_factory.hh"
#include "independent_variable_field_evaluator.hh"

#include "MatrixMFD_Factory.hh"
#include "upwind_potential_difference.hh"
#include "upwind_total_flux.hh"

#include "snow_distribution.hh"

namespace Amanzi {
namespace Flow {

#define DEBUG_FLAG 1

// -------------------------------------------------------------
// Constructor
// -------------------------------------------------------------
void SnowDistribution::setup(const Teuchos::Ptr<State>& S) {
  PKPhysicalBDFBase::setup(S);
  SetupSnowDistribution_(S);
  SetupPhysicalEvaluators_(S);
}


void SnowDistribution::SetupSnowDistribution_(const Teuchos::Ptr<State>& S) {
  // precip function
  Teuchos::ParameterList& precip_func = plist_->sublist("precipitation function");
  FunctionFactory fac;
  precip_func_ = Teuchos::rcp(fac.Create(precip_func));

  // Require fields and evaluators for those fields.
  S->RequireField(key_, name_)->SetMesh(mesh_)->SetGhosted()
      ->SetComponent("cell", AmanziMesh::CELL, 1);

  // -- cell volume and evaluator
  S->RequireFieldEvaluator("surface_cell_volume");

  // Create the upwinding method.
  S->RequireField("upwind_snow_conductivity", name_)->SetMesh(mesh_)
    ->SetGhosted()->SetComponent("face", AmanziMesh::FACE, 1);
  S->GetField("upwind_snow_conductivity",name_)->set_io_vis(false);

  std::string method_name = plist_->get<std::string>("upwind conductivity method",
          "upwind with total flux");
  if (method_name == "upwind with total flux") {
    upwind_method_ = Operators::UPWIND_METHOD_TOTAL_FLUX;
    upwinding_ = Teuchos::rcp(new Operators::UpwindTotalFlux(name_,
          "snow_conductivity", "upwind_snow_conductivity",
          "surface_snow_flux_direction", 1.e-8));
  } else if (method_name == "upwind by potential difference") {
    upwind_method_ = Operators::UPWIND_METHOD_POTENTIAL_DIFFERENCE;
    upwinding_ = Teuchos::rcp(new Operators::UpwindPotentialDifference(name_,
            "snow_conductivity", "upwind_snow_conductivity",
            "snow_skin_potential", "precipitation_snow"));
  } else {
    std::stringstream messagestream;
    messagestream << "Snow Precipitation: has no upwinding method named: " << method_name;
    Errors::Message message(messagestream.str());
    Exceptions::amanzi_throw(message);
  }

  // -- owned secondary variables, no evaluator used
  if (upwind_method_ == Operators::UPWIND_METHOD_TOTAL_FLUX) {
    S->RequireField("surface_snow_flux_direction", name_)->SetMesh(mesh_)->SetGhosted()
        ->SetComponent("face", AmanziMesh::FACE, 1);
  }

  // operator for the diffusion terms: must use ScaledConstraint version
  Teuchos::ParameterList mfd_plist = plist_->sublist("Diffusion");
  mfd_plist.set("TPFA", true);
  mfd_plist.set("scaled constraint equation", true);
  matrix_ = Operators::CreateMatrixMFD(mfd_plist, mesh_);

  matrix_->set_symmetric(false);
  matrix_->SymbolicAssembleGlobalMatrices();
  matrix_->CreateMFDmassMatrices(Teuchos::null);
  matrix_->InitPreconditioner();

  Teuchos::ParameterList mfd_pc_plist = plist_->sublist("Diffusion PC");
  mfd_pc_plist.set("TPFA", true);
  mfd_pc_plist.set("scaled constraint equation", true);
  full_jacobian_ = mfd_pc_plist.get<bool>("TPFA use full Jacobian", false);

  mfd_preconditioner_ = Operators::CreateMatrixMFD(mfd_pc_plist, mesh_);
  mfd_preconditioner_->set_symmetric(false);
  mfd_preconditioner_->SymbolicAssembleGlobalMatrices();
  mfd_preconditioner_->CreateMFDmassMatrices(Teuchos::null);
  mfd_preconditioner_->InitPreconditioner();
};


// -------------------------------------------------------------
// Create the physical evaluators for water content, water
// retention, rel perm, etc, that are specific to Richards.
// -------------------------------------------------------------
void SnowDistribution::SetupPhysicalEvaluators_(const Teuchos::Ptr<State>& S) {
  // -- evaluator for potential field, h + z
  S->RequireFieldEvaluator("snow_skin_potential");
  S->RequireField("snow_skin_potential")->SetMesh(S->GetMesh("surface"))->SetGhosted()
      ->AddComponent("cell", AmanziMesh::CELL, 1);

  // -- conductivity evaluator
  S->RequireFieldEvaluator("snow_conductivity");
  S->RequireField("snow_conductivity")->SetMesh(mesh_)->SetGhosted()
      ->AddComponent("cell", AmanziMesh::CELL, 1);

}


// -------------------------------------------------------------
// Initialize PK
// -------------------------------------------------------------
void SnowDistribution::initialize(const Teuchos::Ptr<State>& S) {
  // Initialize BDF stuff and physical domain stuff.
  PKPhysicalBDFBase::initialize(S);

  // Set extra fields as initialized -- these don't currently have evaluators.
  S->GetFieldData("upwind_snow_conductivity",name_)->PutScalar(1.0);
  S->GetField("upwind_snow_conductivity",name_)->set_initialized();

  if (upwind_method_ == Operators::UPWIND_METHOD_TOTAL_FLUX) {
    S->GetFieldData("surface_snow_flux_direction", name_)->PutScalar(0.);
    S->GetField("surface_snow_flux_direction", name_)->set_initialized();
  }
};


// -----------------------------------------------------------------------------
// Use the physical rel perm (on cells) to update a work vector for rel perm.
//
//   This deals with upwinding, etc.
// -----------------------------------------------------------------------------
bool SnowDistribution::UpdatePermeabilityData_(const Teuchos::Ptr<State>& S) {
  bool update_perm = S->GetFieldEvaluator("snow_conductivity")
      ->HasFieldChanged(S, name_);
  update_perm |= S->GetFieldEvaluator("precipitation_snow")->HasFieldChanged(S, name_);
  update_perm |= S->GetFieldEvaluator("snow_skin_potential")->HasFieldChanged(S, name_);

  if (update_perm) {
    if (upwind_method_ == Operators::UPWIND_METHOD_TOTAL_FLUX) {
      // update the direction of the flux -- note this is NOT the flux
      Teuchos::RCP<CompositeVector> flux_dir =
          S->GetFieldData("surface_snow_flux_direction", name_);

      // Create the stiffness matrix without a rel perm (just n/mu)
      matrix_->CreateMFDstiffnessMatrices(Teuchos::null);

      // Derive the flux
      Teuchos::RCP<const CompositeVector> potential = S->GetFieldData("snow_skin_potential");
      matrix_->DeriveFlux(*potential, flux_dir.ptr());
    }

    // get conductivity data
    const Epetra_MultiVector& cond_c = *S->GetFieldData("snow_conductivity")
        ->ViewComponent("cell",false);

    // get upwind conductivity data
    Teuchos::RCP<CompositeVector> uw_cond =
        S->GetFieldData("upwind_snow_conductivity", name_);

    { // place interior cells on boundary faces
      Epetra_MultiVector& uw_cond_f = *uw_cond->ViewComponent("face",false);

      int nfaces = uw_cond_f.MyLength();
      AmanziMesh::Entity_ID_List cells;
      for (int f=0; f!=nfaces; ++f) {
        mesh_->face_get_cells(f, AmanziMesh::USED, &cells);
        if (cells.size() == 1) {
          int c = cells[0];
          uw_cond_f[0][f] = cond_c[0][c];
        }
      }
    }

    // Then upwind.  This overwrites the boundary if upwinding says so.
    upwinding_->Update(S);
  }

  return update_perm;
}


} // namespace
} // namespace
