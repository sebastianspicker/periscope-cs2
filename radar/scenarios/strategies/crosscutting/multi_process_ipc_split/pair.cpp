#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  auto red = examples::multi_process_ipc_split::apply(w);
  auto blue = examples::multi_process_ipc_split::detect(w);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.60;
  r.summary = red.detail + " | blue=" + std::to_string(blue.signals) +
              "sig risk=" + std::to_string(blue.risk);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}
}  // namespace

StrategyEntry entry_65_multi_process_ipc_split() {
  return {{"65_multi_process_ipc_split", "multi-process IPC split", Family::Evasion, "T0",
           "Red splits cheat into 3 IPC processes: holder, reader, UI",
           "Blue detects via section IPC + handle-process mismatch"}, run};
}
}  // namespace strategies
