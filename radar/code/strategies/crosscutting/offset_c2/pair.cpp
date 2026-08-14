#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `offset_c2`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::offset_c2::apply(w);
  const auto blue = examples::offset_c2::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_14_offset_c2() {
  return {{ "14_offset_c2", "offset c2 schema product", Family::Evasion, "all",
           "Red: remote offset/schema C2 fetch + versioned cache apply",
           "Blue: offset_c2 net + schema_remote + handle multi-reason"},
          run};
}
}  // namespace strategies
