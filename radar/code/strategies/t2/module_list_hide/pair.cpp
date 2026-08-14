// module_list_hide — multi-step PEB unlink; multi-reason PE/Ldr sensors.

#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red, "Multi-step: map module then unlink from PEB Ldr lists.");
  strategy::t2_module_list_hide::Red red;
  red.apply(w);

  n.say(sim::Side::Blue,
        "Multi-reason: shadow scar + PE residual + unlinked modules + foreign thread.");
  strategy::t2_module_list_hide::Blue blue;
  auto det = blue.detect(w);
  auto mit = blue.mitigate(w);

  StrategyResult r;
  r.red_achieved = w.module_shadowing_active || w.module_loaded;
  r.blue_detected = det.detected || det.signals >= 2;
  r.blue_mitigated = mit.mitigated || mit.detection_count >= 2;
  r.summary = det.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_120_module_list_hide() {
  return {{"120_module_list_hide", "Module List Snapshot Evasion",
           Family::Evasion, "T2",
           "Multi-step PEB Ldr unlink of mapped cheat module",
           "Multi-reason shadow/PE/unlinked/thread sensors"},
          run};
}

}  // namespace strategies
