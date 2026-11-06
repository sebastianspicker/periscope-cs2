#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::staged_loader::apply(w);
  const auto blue = examples::staged_loader::detect(w);
  StrategyResult r{red.achieved, blue.detected, blue.mitigated, red.detail + " | " + blue.detail};
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}
}  // namespace
StrategyEntry entry_111_staged_loader() {
  return {{"111_staged_loader", "staged loader", Family::Delivery, "all",
           "Model a staged hand-off without network or code execution",
           "Correlate stage metadata with memory-only payload artifacts"}, run};
}
}  // namespace strategies
