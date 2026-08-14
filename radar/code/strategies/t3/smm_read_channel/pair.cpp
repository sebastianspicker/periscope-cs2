#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::smm_read_channel::apply(w);
  const auto blue = examples::smm_read_channel::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.8;
  result.summary = red.detail + " | blue signals=" +
                   std::to_string(blue.signals) +
                   " risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}

}  // namespace

StrategyEntry entry_62_smm_read_channel() {
  return {{"62_smm_read_channel", "smm read channel", Family::Delivery, "T3",
           "Red plants SMM handler for ring -2 memory read, bypassing HV",
           "Blue detects via SMM residual + HV anomaly, mitigation requires firmware support"},
          run};
}

}  // namespace strategies
