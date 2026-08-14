#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dkom_token_steal::apply(w);
  const auto blue = examples::dkom_token_steal::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.75;
  result.summary = red.detail + " | blue signals=" +
                   std::to_string(blue.signals) +
                   " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}

}  // namespace

StrategyEntry entry_61_dkom_token_steal() {
  return {{"61_dkom_token_steal", "dkom token steal", Family::Delivery, "T2",
           "Red steals SYSTEM token via DKOM for unrestricted handle",
           "Blue detects via token anomaly + full-access handle monitoring"},
          run};
}

}  // namespace strategies
