#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `silent_aim_desync`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::silent_aim_desync::apply(w);
  const auto blue = examples::silent_aim_desync::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_62_silent_aim_desync() {
  return {{ "62_silent_aim_desync", "silent aim desync", Family::Feature, "all",
           "Red multi-step lab path for silent_aim_desync",
           "Blue multi-reason lab path for silent_aim_desync"},
          run};
}
}  // namespace strategies
