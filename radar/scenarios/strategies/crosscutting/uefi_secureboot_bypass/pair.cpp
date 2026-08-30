#include "strategies/pair_util.hpp"

#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  examples::uefi_secureboot_bypass::Red red_actor;
  examples::uefi_secureboot_bypass::Blue blue_actor;
  auto red = red_actor.apply(w);
  auto blue = blue_actor.detect(w);
  auto mitigated = blue.detected ? blue_actor.mitigate(w) : blue;

  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = mitigated.mitigated;
  r.summary = red.detail + " | blue signals=" + std::to_string(blue.signals) +
              " mitigated=" + std::to_string(r.blue_mitigated);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_150_uefi_secureboot_bypass() {
  return {{"150_uefi_secureboot_bypass", "UEFI Secure Boot bypass", Family::Evasion, "T2",
           "Red disables Secure Boot or installs a custom db entry to load unsigned drivers",
           "Blue monitors UEFI variables, verifies driver signatures, and re-enables Secure Boot via firmware callback"},
          run};
}

}  // namespace strategies
