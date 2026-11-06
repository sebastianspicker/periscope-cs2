#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `parent_lineage`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::parent_lineage::apply(w);
  const auto blue = examples::parent_lineage::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_31_parent_lineage() {
  return {{ "31_parent_lineage", "parent lineage", Family::Delivery, "T1",
           "Red multi-step lab path for parent_lineage",
           "Blue multi-reason lab path for parent_lineage"},
          run};
}
}  // namespace strategies
