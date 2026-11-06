#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  StrategyResult result;
  result.red_achieved = diagnostic_telemetry_red_apply(w, n);
  result.blue_detected = diagnostic_telemetry_blue_detect(w, n);
  result.blue_mitigated = result.blue_detected;
  result.summary = result.blue_detected
      ? "Message 159 diagnostics collected multi-reason module/PE/thread evidence"
      : "Message 159 diagnostics found insufficient multi-reason residual";
  n.result(result.blue_detected, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_105_diagnostic_telemetry() {
  return {{"105_diagnostic_telemetry", "CS2 Diagnostic Telemetry", Family::Detection, "T2",
           "Create multiple diagnostic telemetry scars", "Collect Message 159 system diagnostic telemetry"}, run};
}
}  // namespace strategies
