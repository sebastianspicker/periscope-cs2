#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  StrategyResult result;
  result.red_achieved = thread_monitor_red_apply(w, n);
  result.blue_detected = thread_monitor_blue_detect(w, n);
  result.blue_mitigated = result.blue_detected;
  result.summary = result.blue_detected
      ? "Multi-reason: system trampoline + obfuscation/chain residual"
      : "Thread start passes multi-reason start-address checks";
  n.result(result.blue_detected, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_103_thread_monitor_evade() {
  return {{"103_thread_monitor_evade", "CS2 Thread Start Monitor", Family::Detection, "T1",
           "Use a module-backed simulated thread start", "Check captured thread start-address provenance"}, run};
}
}  // namespace strategies
