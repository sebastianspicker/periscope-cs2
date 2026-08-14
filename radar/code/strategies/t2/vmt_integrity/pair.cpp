#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  StrategyResult result;
  result.red_achieved = vmt_integrity_red_apply(w, n);
  result.blue_detected = vmt_integrity_blue_detect(w, n);
  result.blue_mitigated = result.blue_detected;
  result.summary = result.blue_detected
      ? "Multi-reason VMT inventory/CRC/foreign-owner residual"
      : "VMT inventory did not find multi-reason altered targets";
  n.result(result.blue_detected, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_104_vmt_integrity() {
  return {{"104_vmt_integrity", "CS2 VMT Integrity", Family::Detection, "T2",
           "Modify one simulated interface VMT target", "Collect VMT inventory and validate target provenance"}, run};
}
}  // namespace strategies
