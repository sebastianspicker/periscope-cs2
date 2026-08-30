#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  auto red = examples::scattered_entity_read::apply(w);
  auto blue = examples::scattered_entity_read::detect(w);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.55;
  r.summary = red.detail + " | blue=" + std::to_string(blue.signals) +
              "sig risk=" + std::to_string(blue.risk);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}
}  // namespace

StrategyEntry entry_64_scattered_entity_read() {
  return {{"64_scattered_entity_read", "scattered entity read", Family::Evasion, "T0",
           "Red reads entities in random order with timing jitter",
           "Blue detects via handle + scatter pattern (harder than sequential)"}, run};
}
}  // namespace strategies
