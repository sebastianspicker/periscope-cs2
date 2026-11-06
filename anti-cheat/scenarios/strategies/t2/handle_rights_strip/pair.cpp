#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T2 delivery/evasion pair. Red reads the game through a kernel attach
// (KeStackAttachProcess-style) that never opens a handle, so blue's
// ObRegisterCallbacks-style rights strip cannot see or downgrade it. Blue
// detects the attach scar with independent reasons and arms the strip anyway.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::handle_rights_strip::apply(w);
  const auto blue = examples::handle_rights_strip::detect(w);
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

StrategyEntry entry_154_handle_rights_strip() {
  return {{"154_handle_rights_strip", "handle rights strip vs attach read", Family::Delivery,
           "T2",
           "Red reads via kernel attach with no handle, bypassing handle-right strips",
           "Blue arms ObRegisterCallbacks-style rights strip; detects attach-read scar"},
          run};
}
}  // namespace strategies
