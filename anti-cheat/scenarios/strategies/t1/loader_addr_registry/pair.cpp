#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T1 delivery pair. The live binary ships no scanner: a loader resolves
// entity addresses out-of-process into an encrypted registry section that is
// decoded at runtime, and re-resolves after offset drift. Blue scores the
// data-section patch scar and the decode-time access burst.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::loader_addr_registry::apply(w);
  const auto blue = examples::loader_addr_registry::detect(w);
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

StrategyEntry entry_158_loader_addr_registry() {
  return {{"158_loader_addr_registry", "loader-resolved encrypted address registry", Family::Delivery,
           "T1",
           "Red ships no scanner; addresses resolved out-of-process into an encrypted section",
           "Blue detects data-section patching and decode-time access bursts"},
          run};
}
}  // namespace strategies
