#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  auto red = examples::vmexit_keylog_capture::apply(w);
  auto blue = examples::vmexit_keylog_capture::detect(w);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.75;
  r.summary = red.detail + " | blue=" + std::to_string(blue.signals) +
              "sig risk=" + std::to_string(blue.risk);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_71_vmexit_keylog_capture() {
  return {{"71_vmexit_keylog_capture", "VM-exit keylog capture", Family::Feature, "T3",
           "Red captures keystrokes via VM-exit interception, invisible to guest OS",
           "Blue detects via HV probe + bridge driver correlation (extremely hard)"}, run};
}

}  // namespace strategies
