#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `mapper_artifact`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::mapper_artifact::apply(w);
  const auto blue = examples::mapper_artifact::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_66_mapper_artifact() {
  return {{ "66_mapper_artifact", "mapper artifact", Family::Delivery, "T1",
           "Red multi-step lab path for mapper_artifact",
           "Blue multi-reason lab path for mapper_artifact"},
          run};
}
}  // namespace strategies
