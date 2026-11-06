#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `clipcursor`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::clipcursor::apply(w);
  const auto blue = examples::clipcursor::detect(w);

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

StrategyEntry entry_97_clipcursor() {
  return {{ "97_clipcursor", "clipcursor", Family::Delivery, "T4",
           "Red multi-step lab path for clipcursor",
           "Blue multi-reason lab path for clipcursor"},
          run};
}
}  // namespace strategies
