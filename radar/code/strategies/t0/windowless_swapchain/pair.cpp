#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::windowless_swapchain::apply(w);
  const auto blue = examples::windowless_swapchain::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.65;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) + " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}
StrategyEntry entry_57_windowless_swapchain() {
  return {{"57_windowless_swapchain", "windowless swapchain hijack", Family::Delivery, "T0",
"Red hooks game Present without visible overlay",
"Blue detects swapchain hijack via module hook + windowless signature"}, run};
}
}
