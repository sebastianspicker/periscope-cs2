#include "strategies/pair_util.hpp"
#include "red_example.hpp"
#include "blue_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  examples::uefi_txt_measured_boot::Red red;
  red.apply(w);
  const bool red_ok =
      !w.trust.attestation_pcr_ok || !w.trust.secure_launch ||
      w.trust.measured_launch_hidden;

  examples::uefi_txt_measured_boot::Blue blue;
  const auto blue_result = blue.detect(w);
  const auto mitigation =
      blue_result.detected ? blue.mitigate(w) : blue_result;

  StrategyResult r;
  r.red_achieved = red_ok;
  r.blue_detected = blue_result.detected;
  r.blue_mitigated = mitigation.mitigated;
  r.summary = "uefi_txt_measured_boot: signals=" +
              std::to_string(blue_result.signals) +
              " detected=" + std::to_string(r.blue_detected) +
              " mitigated=" + std::to_string(r.blue_mitigated);
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_152_uefi_txt_measured_boot() {
  return {{"152_uefi_txt_measured_boot", "UEFI TXT Measured Boot Evasion",
           Family::Evasion, "crosscutting",
           "Modify TPM event log post-measured-launch to hide early "
           "component. Spoof PCR values to match modified log.",
           "Verify TPM event log integrity: recalculate PCRs from log, "
           "compare against signed quotes, detect entry removal"},
          run};
}

}  // namespace strategies
