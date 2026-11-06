#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T0 handle-attribution pair. Red launders a duplicated handle through a relay
// so the immediate owner is not the origin; blue walks the duplicate chain to
// the root before scoring. The point is that "who owns the handle" is not
// "who started the access".
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::handle_origin_chain::apply(w);
  const auto blue = examples::handle_origin_chain::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.mitigated;
  result.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) +
                   " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}
}  // namespace

StrategyEntry entry_165_handle_origin_chain() {
  return {{"165_handle_origin_chain", "handle origin chain vs owner-only attribution",
           Family::Evasion, "T0",
           "Red launders a duplicated handle through a relay so the owner is not the origin",
           "Blue walks the duplicate chain to the root before scoring the reader"},
          run};
}
}  // namespace strategies
