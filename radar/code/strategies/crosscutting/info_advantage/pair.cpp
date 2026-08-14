#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `info_advantage`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::info_advantage::apply(w);
  const auto blue = examples::info_advantage::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_25_info_advantage() {
  return {{ "25_info_advantage", "info advantage", Family::Structural, "all",
           "Red multi-step lab path for info_advantage",
           "Blue multi-reason lab path for info_advantage"},
          run};
}
}  // namespace strategies
