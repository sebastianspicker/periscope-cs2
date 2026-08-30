#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

// Strategy pair wiring for `web_phone_radar`.

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::web_phone_radar::apply(w);
  const auto blue = examples::web_phone_radar::detect(w);

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_10_web_phone_radar() {
  return {{ "10_web_phone_radar", "web phone radar", Family::Feature, "all",
           "Red multi-step lab path for web_phone_radar",
           "Blue multi-reason lab path for web_phone_radar"},
          run};
}
}  // namespace strategies
