#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::handle_hijack_proxy_donor::apply(w);
  const auto blue = examples::handle_hijack_proxy_donor::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.detected && blue.signals >= 2;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_124_handle_hijack_proxy_donor() {
  return {{"124_handle_hijack_proxy_donor", "donor hijack via NtQSI + DuplicateHandle", Family::Evasion, "T0",
           "Red uses NtQSI+DuplicateHandle via donor process",
           "Blue correlates proxy handle ownership pattern"}, run};
}
}  // namespace strategies
