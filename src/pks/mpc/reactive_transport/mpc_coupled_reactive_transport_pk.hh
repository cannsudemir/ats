#include "PK_Factory.hh"
#include "PK_MPCAdditive.hh"
#include "weak_mpc.hh"
#include "mpc_coupled_transport.hh"

namespace Amanzi {

class Coupled_ReactiveTransport_PK_ATS : public PK_MPCAdditive<PK> {
public:
  Coupled_ReactiveTransport_PK_ATS(Teuchos::ParameterList& pk_tree,
               const Teuchos::RCP<Teuchos::ParameterList>& global_list,
               const Teuchos::RCP<State>& S,
               const Teuchos::RCP<TreeVector>& soln);

  ~Coupled_ReactiveTransport_PK_ATS(){};

  // PK methods
  // -- dt is the minimum of the sub pks
  virtual double get_dt();
  // virtual void set_dt(double dt){};

  // // set States
  // virtual void set_states(const Teuchos::RCP<const State>& S,
  //                         const Teuchos::RCP<State>& S_inter,
  //                         const Teuchos::RCP<State>& S_next){};

  // // -- advance each sub pk dt.
  // virtual bool AdvanceStep(double t_old, double t_new, bool reinit = false){};

  // virtual void Initialize(const Teuchos::Ptr<State>& S);

  // virtual void CommitStep(double t_old, double t_new, const Teuchos::RCP<State>& S);

  std::string name() { return "coupled reactive transport";}

private:
  
  bool chem_step_succeeded;
  bool storage_created;
  double dTtran_, dTchem_;
  int transport_pk_index_, chemistry_pk_index_;
  Teuchos::RCP<CoupledTransport_PK> tranport_pk_;
  Teuchos::RCP<WeakMPC> chemistry_pk_;
  Teuchos::RCP<Teuchos::ParameterList> crt_pk_list_;

  // factory registration
  static RegisteredPKFactory<Coupled_ReactiveTransport_PK_ATS> reg_;
};

}  // namespace Amanzi
