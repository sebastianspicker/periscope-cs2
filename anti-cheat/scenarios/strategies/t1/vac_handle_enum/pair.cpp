#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  StrategyResult result;
  const auto status = vac_handle_red_apply(w, n);
  result.red_achieved = status == ac::Status::Ok;
  result.blue_detected = vac_handle_blue_detect(w, n);
  result.blue_mitigated = result.blue_detected;
  result.summary = result.blue_detected
      ? "Multi-reason: foreign VM_READ co-occurs with remote-read telemetry"
      : "No multi-reason foreign VM_READ residual was observed";
  n.result(result.blue_detected, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_102_vac_handle_enum() {
  return {{"102_vac_handle_enum", "VAC Handle Table Enumeration", Family::Detection, "T0-T1",
           "Open a foreign VM_READ handle to the game",
           "Enumerate foreign VM_READ handles targeting the game"}, run};
}
}  // namespace strategies
