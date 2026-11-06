// handle_inherit_detect — multi-step parent/child inherit; multi-reason blue.

#include "strategies/framework.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.say(sim::Side::Red,
        "Multi-step: parent opens game; child inherits proxy VM_READ handle.");
  strategy::t0_handle_inherit_detect::Red red;
  red.apply(w);

  bool red_ok = false;
  for (const auto& h : w.handles_to(w.game_pid(), true)) {
    if (h.via_proxy && sim::has(h.access, sim::AccessMask::VmRead)) {
      red_ok = true;
      break;
    }
  }

  n.say(sim::Side::Blue,
        "Multi-reason: proxy handle + parent co-hold + lineage + read telemetry.");
  strategy::t0_handle_inherit_detect::Blue blue;
  auto det = blue.detect(w);
  auto mit = blue.mitigate(w);

  StrategyResult r;
  r.red_achieved = red_ok;
  r.blue_detected = det.detected || (det.signals >= 2 && det.inheritance_chain_detected);
  r.blue_mitigated = mit.mitigated ||
                     mit.detail.find("Mitigated") != std::string::npos;
  r.summary = det.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_123_handle_inherit_detect() {
  return {{"123_handle_inherit_detect", "Process Handle Inheritance Detection",
           Family::Detection, "T0",
           "Multi-step parent/child inherited VM_READ chain",
           "Multi-reason inheritance + lineage handle-graph sensors"},
          run};
}

}  // namespace strategies
