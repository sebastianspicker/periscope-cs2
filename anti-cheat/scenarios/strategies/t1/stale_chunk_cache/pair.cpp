#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T1 delivery/evasion pair. Red memoizes container chunk pointers (tagged
// with the generation they were valid at) and serves entity reads from the
// cache; blue deliberately reallocates the container (churn pressure) and
// detects the stale reads as generation-counter mismatches.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::stale_chunk_cache::apply(w);
  const auto blue = examples::stale_chunk_cache::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) +
                   " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_163_stale_chunk_cache() {
  return {{"163_stale_chunk_cache", "stale chunk cache under churn", Family::Delivery,
           "T0-T1",
           "Red memoizes container chunk pointers and reads (stale) chunks after churn",
           "Blue reallocates deliberately and detects stale reads via generation counters"},
          run};
}
}  // namespace strategies
