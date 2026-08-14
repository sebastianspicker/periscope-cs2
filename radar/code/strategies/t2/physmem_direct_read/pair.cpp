#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::physmem_direct_read::apply(w);
  const auto blue = examples::physmem_direct_read::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.7;
  result.summary = red.detail + " | blue signals=" +
                   std::to_string(blue.signals) + " risk=" +
                   std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}

}  // namespace

StrategyEntry entry_60_physmem_direct_read() {
  return {{"60_physmem_direct_read", "physmem direct read", Family::Delivery,
           "T2", "Red maps PhysicalMemory directly, no handle to game",
           "Blue detects via physmem device open + DSE policy check"},
          run};
}

}  // namespace strategies
