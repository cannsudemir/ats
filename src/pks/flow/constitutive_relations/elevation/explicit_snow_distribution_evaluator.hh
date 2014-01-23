/* -*-  mode: c++; c-default-style: "google"; indent-tabs-mode: nil -*- */

/*
  The elevation evaluator gets the surface elevation and slope.

  This is not a normal SecondaryVariablesFieldEvaluator, as it has no
  dependencies, which means we have to force it to update (dependencies
  will never have changed) in HasFieldChanged.  This is done this
  way so that when the mesh changes, this can be updated appropriately.

  Authors: Ethan Coon (ecoon@lanl.gov)
*/

#ifndef AMANZI_FLOWRELATIONS_EXPLICIT_SNOW_DISTRIBUTION_EVALUATOR_
#define AMANZI_FLOWRELATIONS_EXPLICIT_SNOW_DISTRIBUTION_EVALUATOR_

#include "factory.hh"
#include "secondary_variable_field_evaluator.hh"

namespace Amanzi {
class Function;
namespace Operators { class MatrixMFD; }

namespace Flow {
namespace FlowRelations {

class ExplicitSnowDistributionEvaluator : public SecondaryVariableFieldEvaluator {

 public:
  explicit
  ExplicitSnowDistributionEvaluator(Teuchos::ParameterList& plist);

  ExplicitSnowDistributionEvaluator(const ExplicitSnowDistributionEvaluator& other);
  virtual Teuchos::RCP<FieldEvaluator> Clone() const {
    return Teuchos::rcp(new ExplicitSnowDistributionEvaluator(*this));
  }

  // Required methods from SecondaryVariableFieldEvaluator
  virtual void EvaluateField_(const Teuchos::Ptr<State>& S,
          const Teuchos::Ptr<CompositeVector>& result);
  virtual void EvaluateFieldPartialDerivative_(const Teuchos::Ptr<State>& S,
          Key wrt_key, const Teuchos::Ptr<CompositeVector>& result);

protected:
  void AssembleOperator_(const Teuchos::Ptr<State>& S);


 protected:
  Key elev_key_;
  Key slope_key_;
  Key pd_key_;
  Key snow_height_key_;
  Key cell_vol_key_;

  double kL_;
  double kdx_;
  double ktmax_;
  double kS_;
  double kCFL_;
  
  Teuchos::RCP<Function> precip_func_;

  bool assembled_;
  std::string mesh_name_;
  Teuchos::RCP<Operators::MatrixMFD> matrix_;

 private:
  static Utils::RegisteredFactory<FieldEvaluator,ExplicitSnowDistributionEvaluator> factory_;

};

} //namespace
} //namespace
} //namespace

#endif
