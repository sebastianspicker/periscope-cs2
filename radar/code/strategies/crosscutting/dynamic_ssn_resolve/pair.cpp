#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  const auto red = examples::dynamic_ssn_resolve::apply(w);
  const auto blue = examples::dynamic_ssn_resolve::detect(w);
  StrategyResult result;
  result.red_achieved = red.achieved;
  result.blue_detected = blue.detected;
  result.blue_mitigated = blue.risk >= 0.60;
  result.summary = red.detail + " | blue=" + std::to_string(blue.signals) +
                   "sig risk=" + std::to_string(blue.risk);
  n.result(result.blue_detected || result.blue_mitigated, result.summary);
  return result;
}

}  // namespace

StrategyEntry entry_66_dynamic_ssn_resolve() {
  return {{"66_dynamic_ssn_resolve", "dynamic SSN resolve", Family::Evasion, "T1",
           "Red resolves syscall numbers from ntdll on disk, defeats SSN fingerprinting",
           "Blue detects via handle graph + syscall path (SSN fingerprint still possible)"},
          run};
}

}  // namespace strategies
