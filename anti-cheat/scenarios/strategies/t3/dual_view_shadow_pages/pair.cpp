#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

// T3 evasion pair. Extends 48_ept_hide_ac with a dual-view split: the execute
// view serves patched code while the read view keeps reporting pristine
// bytes. Blue probes cross-view consistency with timing, TLB/ITC effects, and
// self-reads; a dual-view shadow cannot keep every probe honest at once.
namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dual_view_shadow_pages::apply(w);
  const auto blue = examples::dual_view_shadow_pages::detect(w);
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

StrategyEntry entry_159_dual_view_shadow_pages() {
  return {{"159_dual_view_shadow_pages", "dual-view shadow pages", Family::Evasion,
           "T2-T3",
           "Red splits execute (patched) from read (pristine) views of the same pages",
           "Blue probes cross-view consistency: timing, TLB/ITC effects, self-reads"},
          run};
}
}  // namespace strategies
