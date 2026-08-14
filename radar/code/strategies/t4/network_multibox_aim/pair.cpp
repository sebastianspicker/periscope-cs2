#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "network_multibox_aim",
         "Secondary box streams aim samples over LAN.");
  const auto red = examples::network_multibox_aim::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "multibox stream",
            "Remote aim samples + input desync residual.");
  const auto blue = examples::network_multibox_aim::detect(w);
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

StrategyEntry entry_55_network_multibox_aim() {
  return {{"55_network_multibox_aim", "network multibox aim", Family::Delivery,
           "T4",
           "Red: remote multibox aim stream + input desync",
           "Blue: multibox net aim residual multi-reason"},
          run};
}

StrategyEntry entry_73_network_multibox_aim() {
  return entry_55_network_multibox_aim();
}

}  // namespace strategies
