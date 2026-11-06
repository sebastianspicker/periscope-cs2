#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `dse_testsign`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dse_testsign::apply(w);
  const auto blue = examples::dse_testsign::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_67_dse_testsign() {
  return {{ "67_dse_testsign", "dse testsign", Family::Delivery, "T1",
           "Red multi-step lab path for dse_testsign",
           "Blue multi-reason lab path for dse_testsign"},
          run};
}
}  // namespace strategies
