#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  examples::adaptive_strategy::Red red;
  red.apply(w);

  examples::adaptive_strategy::Blue blue;
  const auto blue_result = blue.detect(w);
  const auto mitigation = (blue_result.detection_count >= 2) ? blue.mitigate(w) : blue_result;

  StrategyResult r;
  r.red_achieved = true;
  r.blue_detected = blue_result.detection_count >= 2;
  r.blue_mitigated = mitigation.detection_count >= 2;
  r.summary = "adaptive_strategy: " +
              std::to_string(blue_result.detection_count) + " signals" +
              " | mitigated=" + std::to_string(r.blue_mitigated);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_148_adaptive_strategy() {
  return {{"148_adaptive_strategy", "adaptive strategy", Family::Evasion, "crosscutting",
           "Adaptive multi-tier evasion: probes blue sensors, selects optimal tier",
           "Detects adaptive multi-tier evasion via tier-hopping, opsec, and C2 analysis"},
          run};
}
}  // namespace strategies
