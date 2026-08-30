#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `wfp_ndis_filter`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::wfp_ndis_filter::run_red(w, n);
  const auto blue = examples::wfp_ndis_filter::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_81_wfp_ndis_filter() {
  return {{ "81_wfp_ndis_filter", "wfp ndis filter", Family::Delivery, "T2",
           "Red multi-step lab path for wfp_ndis_filter",
           "Blue multi-reason lab path for wfp_ndis_filter"},
          run};
}
}  // namespace strategies
