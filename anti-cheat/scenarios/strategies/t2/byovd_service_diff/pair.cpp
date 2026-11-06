#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T2 cleanup-completeness pair. Red deletes the SCM service after using a
// vulnerable driver; the loaded-module hash survives until reboot. Blue treats
// the module inventory as authoritative and flags the orphan.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::byovd_service_diff::apply(w);
  const auto blue = examples::byovd_service_diff::detect(w);
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

StrategyEntry entry_167_byovd_service_diff() {
  return {{"167_byovd_service_diff", "BYOVD service cleanup vs module-hash orphan",
           Family::Detection, "T2",
           "Red uses a vulnerable driver then deletes the SCM service and registry key",
           "Blue hashes loaded modules and flags the orphaned known-bad hash"},
          run};
}
}  // namespace strategies
