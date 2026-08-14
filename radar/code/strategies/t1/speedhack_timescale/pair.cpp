#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `speedhack_timescale`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::speedhack_timescale::apply(w);
  const auto blue = examples::speedhack_timescale::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_79_speedhack_timescale() {
  return {{ "79_speedhack_timescale", "speedhack timescale", Family::Delivery, "T1",
           "Red multi-step lab path for speedhack_timescale",
           "Blue multi-reason lab path for speedhack_timescale"},
          run};
}
}  // namespace strategies
