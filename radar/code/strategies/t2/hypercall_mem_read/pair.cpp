#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::hypercall_mem_read::apply(w);
  const auto blue = examples::hypercall_mem_read::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.70;
  result.summary = red.detail + " | blue=" +
                   std::to_string(blue.signals) + "sig risk=" +
                   std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}

}  // namespace

StrategyEntry entry_69_hypercall_mem_read() {
  return {{"69_hypercall_mem_read", "hypercall memory read", Family::Delivery,
           "T2", "Red uses platform hypervisor hypercalls to read physical memory, no driver",
           "Blue detects via HV-level monitoring + hypercall interface analysis"},
          run};
}

}  // namespace strategies
