#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `thread_hide_dbg`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::thread_hide_dbg::apply(w);
  const auto blue = examples::thread_hide_dbg::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.risk >= 0.72;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_78_thread_hide_dbg() {
  return {{ "78_thread_hide_dbg", "thread hide dbg", Family::Delivery, "T1",
           "Red multi-step lab path for thread_hide_dbg",
           "Blue multi-reason lab path for thread_hide_dbg"},
          run};
}
}  // namespace strategies
