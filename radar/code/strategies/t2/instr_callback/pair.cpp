#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `instr_callback`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::instr_callback::run_red(w, n);
  const auto blue = examples::instr_callback::run_blue(w, n);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_92_instr_callback() {
  return {{ "92_instr_callback", "instr callback", Family::Delivery, "T2",
           "Red multi-step lab path for instr_callback",
           "Blue multi-reason lab path for instr_callback"},
          run};
}
}  // namespace strategies
