#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "lag_switch",
         "Intentional drop bursts; not steady packet loss.");
  const auto red = examples::lag_switch::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "burst disambig",
            "Score lag-switch bursts vs packet_loss_faked.");
  const auto blue = examples::lag_switch::detect(w);
  n.say(sim::Side::Blue, blue.detail);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.65;
  r.summary = red.detail + " | " + blue.detail + " | signals=" +
              std::to_string(blue.signals) + " risk=" + std::to_string(blue.risk);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_54_lag_switch() {
  return {{"54_lag_switch", "lag switch", Family::Delivery, "T4",
           "Red: intentional network drop bursts (lag switch residual)",
           "Blue: burst residual vs packet_loss_faked disambiguation"},
          run};
}

// Keep old symbol name if any dormant refs exist (alias).
StrategyEntry entry_84_lag_switch() { return entry_54_lag_switch(); }

}  // namespace strategies
