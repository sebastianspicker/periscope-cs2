#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

#include "fps/scenario.hpp"

// Catalog pair: CS dusty_yard round → lab_bridge → RPM red → interest fog blue.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  fps::RoundConfig cfg;
  cfg.freeze_time = 0.f;
  fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
  sc.start_default_round();

  n.move(sim::Side::Red, "cs_round_radar",
         "Plant A on dusty_yard; bridge players into World; RPM entity product.");
  auto red = examples::cs_round_radar::apply_from_scenario(sc, w);
  n.say(sim::Side::Red, red.detail);

  n.counter(sim::Side::Blue, "interest fog+handle",
            "CT-side observer culls far T-spawn enemies; cut client fidelity.");
  auto blue = examples::cs_round_radar::detect_with_scenario(w, sc);
  n.say(sim::Side::Blue, blue.detail);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_32_cs_round_radar() {
  return {{"32_cs_round_radar", "cs round radar fog", Family::Structural, "T0",
           "CS scenario plant → lab_bridge entity table → external RPM",
           "Handle residual + interest fog cull far site enemies + fidelity cut"},
          run};
}

}  // namespace strategies
