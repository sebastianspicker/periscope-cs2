#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `minifilter_strip`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::minifilter_strip::run_red(w, n);
  const auto blue = examples::minifilter_strip::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_56_minifilter_strip() {
  return {{ "56_minifilter_strip", "minifilter strip", Family::Delivery, "T2",
           "Red multi-step lab path for minifilter_strip",
           "Blue multi-reason lab path for minifilter_strip"},
          run};
}
}  // namespace strategies
