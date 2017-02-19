/*
  This is the mpc_pk component of the Amanzi code. 

  Copyright 2010-201x held jointly by LANS/LANL, LBNL, and PNNL. 
  Amanzi is released under the three-clause BSD License. 
  The terms of use and "as is" disclaimer for this license are 
  provided in the top-level COPYRIGHT file.

  Authors: Daniil Svyatskiy

  Process kernel for coupling of Transport PK and Chemistry PK.
*/

#include "mpc_coupled_reactive_transport_pk.hh"

namespace Amanzi { 

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
Coupled_ReactiveTransport_PK_ATS::Coupled_ReactiveTransport_PK_ATS(
                                          Teuchos::ParameterList& pk_tree,
                                          const Teuchos::RCP<Teuchos::ParameterList>& global_list,
                                          const Teuchos::RCP<State>& S,
                                          const Teuchos::RCP<TreeVector>& soln) :
  Amanzi::PK_MPCAdditive<PK>(pk_tree, global_list, S, soln)
 { 

  storage_created = false;
  chem_step_succeeded = true;
  std::string pk_name = pk_tree.name();
// Create miscaleneous lists.
  Teuchos::RCP<Teuchos::ParameterList> pk_list = Teuchos::sublist(global_list, "PKs", true);
  //std::cout<<*pk_list;
  crt_pk_list_ = Teuchos::sublist(pk_list, pk_name, true);
  
  transport_pk_index_ = crt_pk_list_->get<int>("transport index", 0);
  chemistry_pk_index_ = crt_pk_list_->get<int>("chemistry index", 1 - transport_pk_index_);

  tranport_pk_ = Teuchos::rcp_dynamic_cast<CoupledTransport_PK>(sub_pks_[transport_pk_index_]);
  ASSERT(tranport_pk_ != Teuchos::null);
  
  chemistry_pk_ = Teuchos::rcp_dynamic_cast<WeakMPC>(sub_pks_[chemistry_pk_index_]);
  ASSERT(chemistry_pk_ != Teuchos::null);

  // communicate chemistry engine to transport.
#ifdef ALQUIMIA_ENABLED
  // tranport_pk_->SetupAlquimia(Teuchos::rcp_static_cast<AmanziChemistry::Alquimia_PK>(chemistry_pk_),
  //                             chemistry_pk_->chem_engine());
#endif


 }

// -----------------------------------------------------------------------------
// Calculate the min of sub PKs timestep sizes.
// -----------------------------------------------------------------------------
double Coupled_ReactiveTransport_PK_ATS::get_dt() {

  dTtran_ = tranport_pk_->get_dt();
  dTchem_ = chemistry_pk_->get_dt();

  if (!chem_step_succeeded && (dTchem_/dTtran_ > 0.99)) {
     dTchem_ *= 0.5;
  } 

  if (dTtran_ > dTchem_) dTtran_= dTchem_; 
  
  return dTchem_;
}

}