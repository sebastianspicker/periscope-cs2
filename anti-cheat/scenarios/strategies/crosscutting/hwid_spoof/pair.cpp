#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {
StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::hwid_spoof::apply(w);
  const auto blue = examples::hwid_spoof::detect(w);
  StrategyResult r{red.achieved, blue.detected, blue.mitigated, red.detail + " | " + blue.detail};
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}
}  // namespace
StrategyEntry entry_110_hwid_spoof() {
  return {{"110_hwid_spoof", "hardware identity spoof", Family::Evasion, "all",
           "Model a mismatched hardware-identity profile", "Correlate identity components and account history"}, run};
}
}  // namespace strategies
