#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 delivery/evasion strategy pair. Red plants GDI BitBlt capture; blue correlates
// channel telemetry with that scar rather than relying on process names alone.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::gdi_bitblt::apply(w);
  const auto blue = examples::gdi_bitblt::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 3;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_76_gdi_bitblt() {
  return {{"76_gdi_bitblt", "gdi bitblt", Family::Delivery, "T0",
"Red plants GDI BitBlt capture",
"Blue correlates telemetry with GDI BitBlt capture"}, run};
}
}  // namespace strategies
