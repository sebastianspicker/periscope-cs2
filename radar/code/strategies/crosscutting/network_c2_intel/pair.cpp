#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `network_c2_intel`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::network_c2_intel::apply(w);
  const auto blue = examples::network_c2_intel::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_28_network_c2_intel() {
  return {{ "28_network_c2_intel", "network c2 intel", Family::Detection, "all",
           "Red multi-step lab path for network_c2_intel",
           "Blue multi-reason lab path for network_c2_intel"},
          run};
}
}  // namespace strategies
