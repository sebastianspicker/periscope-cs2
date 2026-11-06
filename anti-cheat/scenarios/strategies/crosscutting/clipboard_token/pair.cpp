#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `clipboard_token`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::clipboard_token::apply(w);
  const auto blue = examples::clipboard_token::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_99_clipboard_token() {
  return {{ "99_clipboard_token", "clipboard token", Family::Evasion, "all",
           "Red multi-step lab path for clipboard_token",
           "Blue multi-reason lab path for clipboard_token"},
          run};
}
}  // namespace strategies
