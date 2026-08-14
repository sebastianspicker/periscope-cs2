#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::helper_ticket_protocol::apply(w);
  const auto blue = examples::helper_ticket_protocol::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_139_helper_ticket_protocol() {
  return {{"139_helper_ticket_protocol", "shared memory helper ticket protocol", Family::Delivery, "T0",
           "Red creates shared memory ticket for helper-radar IPC",
           "Blue detects shared memory sections carrying entity bytes"}, run};
}
}  // namespace strategies
