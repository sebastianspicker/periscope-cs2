#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "composition_radar_loop",
         "scan → layout mutate → refresh → overlay product");
  auto red = examples::composition_radar_loop::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "stack+fog",
            "Pattern multi-reason + interest fog+ fidelity budget");
  auto blue = examples::composition_radar_loop::detect(w);
  std::string blue_detail;
  for (const auto& re : blue.reasons)
    blue_detail += re + "; ";
  n.say(sim::Side::Blue, blue_detail);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue_detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}
}
StrategyEntry entry_31_composition_radar_loop() {
  return {{"31_composition_radar_loop", "composition scan refresh fog",
           Family::Structural, "T0",
           "Pattern scan + auto-refresh + overlay on one session",
           "Multi-sensor pattern blue + structural fog+ starve"},
          run};
}
}  // namespace strategies
