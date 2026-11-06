#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `delayed_ban`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::delayed_ban::apply(w);
  const auto blue = examples::delayed_ban::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_26_delayed_ban() {
  return {{ "26_delayed_ban", "delayed ban", Family::Structural, "all",
           "Red multi-step lab path for delayed_ban",
           "Blue multi-reason lab path for delayed_ban"},
          run};
}
}  // namespace strategies
