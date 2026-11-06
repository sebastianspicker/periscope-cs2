#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `input_synthesis`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::input_synthesis::apply(w);
  const auto blue = examples::input_synthesis::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_19_input_synthesis() {
  return {{ "19_input_synthesis", "input synthesis", Family::Feature, "all",
           "Red multi-step lab path for input_synthesis",
           "Blue multi-reason lab path for input_synthesis"},
          run};
}
}  // namespace strategies
