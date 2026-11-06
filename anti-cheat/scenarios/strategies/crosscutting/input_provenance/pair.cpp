#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `input_provenance`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::input_provenance::apply(w);
  const auto blue = examples::input_provenance::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_22_input_provenance() {
  return {{ "22_input_provenance", "input provenance", Family::Detection, "all",
           "Red multi-step lab path for input_provenance",
           "Blue multi-reason lab path for input_provenance"},
          run};
}
}  // namespace strategies
