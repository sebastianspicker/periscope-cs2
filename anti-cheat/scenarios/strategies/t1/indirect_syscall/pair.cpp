#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `indirect_syscall`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::indirect_syscall::apply(w);
  const auto blue = examples::indirect_syscall::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected || blue.signals >= 2;
  r.blue_mitigated = blue.mitigated || blue.risk >= 0.72 || blue.signals >= 3;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals);
  n.result(r.blue_detected, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_02_indirect_syscall() {
  return {{ "02_indirect_syscall", "indirect syscall", Family::Delivery, "T1",
           "Red multi-step lab path for indirect_syscall",
           "Blue multi-reason lab path for indirect_syscall"},
          run};
}
}  // namespace strategies
