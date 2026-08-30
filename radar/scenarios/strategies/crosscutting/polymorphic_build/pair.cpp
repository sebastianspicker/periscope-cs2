#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::polymorphic_build::apply(w);
  const auto blue = examples::polymorphic_build::detect(w);
  StrategyResult r{red.achieved, blue.detected, blue.mitigated, red.detail + " | " + blue.detail};
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}
}  // namespace
StrategyEntry entry_107_polymorphic_build() {
  return {{"107_polymorphic_build", "polymorphic build", Family::Evasion, "all",
           "Model per-build layout variation without producing a binary",
           "Profile stable lineage artifacts instead of one file hash"}, run};
}
}  // namespace strategies
