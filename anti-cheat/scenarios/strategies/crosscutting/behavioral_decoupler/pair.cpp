#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  examples::behavioral_decoupler::Red red;
  red.apply(w);

  examples::behavioral_decoupler::Blue blue;
  const auto blue_result = blue.detect(w);
  const auto mitigation = (blue_result.detection_count >= 2) ? blue.mitigate(w) : blue_result;

  StrategyResult r;
  r.red_achieved = true;
  r.blue_detected = blue_result.detection_count >= 2;
  r.blue_mitigated = mitigation.detection_count >= 2;
  r.summary = "behavioral_decoupler: " +
              std::to_string(blue_result.detection_count) + " signals" +
              " | mitigated=" + std::to_string(r.blue_mitigated);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_149_behavioral_decoupler() {
  return {{"149_behavioral_decoupler", "behavioral decoupler", Family::Evasion, "crosscutting",
           "Behavioral decoupling: frame skip, entity omission, blind spots, position fuzzing",
           "Detects behavioral decoupling via skip patterns, latency consistency, fuzz detection, and VACnet risk"},
          run};
}
}  // namespace strategies
