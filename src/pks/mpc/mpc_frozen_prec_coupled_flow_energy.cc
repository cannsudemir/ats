/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */
/* -------------------------------------------------------------------------
ATS

License: see $ATS_DIR/COPYRIGHT
Author: Ethan Coon

Derived MPC for flow and energy.  This couples using a block-diagonal coupler.
------------------------------------------------------------------------- */

#include "wrm_richards_evaluator.hh"
#include "mpc_frozen_prec_coupled_flow_energy.hh"

namespace Amanzi {

#define DEBUG_FLAG 1

RegisteredPKFactory<MPCFrozenCoupledFlowEnergy> MPCFrozenCoupledFlowEnergy::reg_("frozen energy-flow preconditioner coupled");

bool MPCFrozenCoupledFlowEnergy::modify_predictor(double h, Teuchos::RCP<TreeVector> u) {
  // modification of the initial guess occurs by updating T using this guess,
  // then calculating the p that would be required to keep the same water
  // mass.

  // Stuff temperature into state
  Teuchos::RCP<TreeVector> uT = u->SubVector("energy");
  sub_pks_[1]->solution_to_state(uT, S_next_);

#if DEBUG_FLAG
  std::cout << std::endl;
  std::cout << "Modifying guess:" << std::endl;
  std::cout << "  T0: " << (*uT->data())("cell",0) << std::endl;
  std::cout << "  T1: " << (*uT->data())("cell",99) << std::endl;
#endif

  // update water content, which will get all the needed vals updated at the new temp.
  S_next_->GetFieldEvaluator("water_content")
      ->HasFieldChanged(S_next_.ptr(), "richards_pk");

  // get the needed vals
  Teuchos::RCP<const CompositeVector> wc0 = S_inter_->GetFieldData("water_content");
  Teuchos::RCP<const CompositeVector> cv = S_inter_->GetFieldData("cell_volume");
  Teuchos::RCP<const CompositeVector> phi = S_next_->GetFieldData("porosity");
  Teuchos::RCP<const CompositeVector> n_g = S_next_->GetFieldData("molar_density_gas");
  Teuchos::RCP<const CompositeVector> omega_g = S_next_->GetFieldData("mol_frac_gas");
  Teuchos::RCP<const CompositeVector> n_l = S_next_->GetFieldData("molar_density_liquid");
  Teuchos::RCP<const CompositeVector> n_i = S_next_->GetFieldData("molar_density_ice");
  Teuchos::RCP<const CompositeVector> one_on_A = S_next_->GetFieldData("wrm_permafrost_one_on_A");
  Teuchos::RCP<const double> p_atm = S_next_->GetScalarData("atmospheric_pressure");

  // get the WRMs
  Teuchos::RCP<FieldEvaluator> wrm_B_eval_base = S_next_->GetFieldEvaluator("wrm_permafrost_one_on_B");
  Teuchos::RCP<Flow::FlowRelations::WRMRichardsEvaluator> wrm_B_eval =
      Teuchos::rcp_dynamic_cast<Flow::FlowRelations::WRMRichardsEvaluator>(wrm_B_eval_base);
  Teuchos::RCP<Flow::FlowRelations::WRMRegionPairList> wrms = wrm_B_eval->get_WRMs();


  // get the result pressure
  Teuchos::RCP<CompositeVector> pres = u->SubVector("flow")->data();
  for (Flow::FlowRelations::WRMRegionPairList::iterator region=wrms->begin();
       region!=wrms->end(); ++region) {
    std::string name = region->first;
    int ncells = pres->mesh()->get_set_size(name, AmanziMesh::CELL, AmanziMesh::OWNED);
    std::vector<int> cells(ncells);
    pres->mesh()->get_set_entities(name, AmanziMesh::CELL, AmanziMesh::OWNED, &cells);

    for (std::vector<int>::iterator c=cells.begin(); c!=cells.end(); ++c) {
      if ((*pres)("cell",*c) < *p_atm) {
        double A_minus_one = (1.0/(*one_on_A)("cell",*c) - 1.0);
        if (*c==0) std::cout << "   A-1(0) = " << A_minus_one << std::endl;
        if (*c==99) std::cout << "   A-1(99) = " << A_minus_one << std::endl;

        double wc = (*wc0)("cell",*c) / (*cv)("cell",*c);
        double sstar = (wc - (*n_g)("cell",*c)*(*omega_g)("cell",*c)*(*phi)("cell",*c)) /
            ((*phi)("cell",*c) * ((*n_l)("cell",*c) - (*n_g)("cell",*c)*(*omega_g)("cell",*c)
                    + (*n_i)("cell",*c)*A_minus_one) - A_minus_one*wc);

        if (*c==0) std::cout << "   S*(0) = " << sstar << std::endl;
        if (*c==99) std::cout << "   S*(99) = " << sstar << std::endl;

        double pc = region->second->capillaryPressure(sstar);

        if (*c==0) std::cout << "   pc(0) = " << pc << std::endl;
        if (*c==99) std::cout << "   pc(99) = " << pc << std::endl;

        (*pres)("cell",*c) = *p_atm - pc;

        if (*c==0) std::cout << "   p(0) = " << (*pres)("cell",*c) << std::endl;
        if (*c==99) std::cout << "   p(99) = " <<  (*pres)("cell",*c) << std::endl;

      }
    }
  }

  return true;
};

} // namespace