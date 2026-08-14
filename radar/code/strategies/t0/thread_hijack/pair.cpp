#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants hijacked game thread; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::thread_hijack::apply(w);
  const auto blue = examples::thread_hijack::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_53_thread_hijack() {
  return {{"53_thread_hijack", "thread hijack", Family::Delivery, "T0",
"Red plants hijacked game thread",
"Blue correlates telemetry with hijacked game thread"}, run};
}
}  // namespace strategies
