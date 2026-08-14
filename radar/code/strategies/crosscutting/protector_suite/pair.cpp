#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "protector_suite",
         "Anti-debug + anti-suspend + destruct + watchdog.");
  const auto red = examples::protector_suite::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "protector residual",
            "Multi-reason protector suite + handle graph.");
  const auto blue = examples::protector_suite::detect(w);
  n.say(sim::Side::Blue, blue.detail);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_45_protector_suite() {
  return {{"45_protector_suite", "protector suite", Family::Evasion, "all",
           "Red: anti-debug / anti-suspend / destruct / watchdog suite",
           "Blue: protector residual multi-reason (≠ generic anti_re alone)"},
          run};
}
}  // namespace strategies
