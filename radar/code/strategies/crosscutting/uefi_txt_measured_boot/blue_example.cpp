#include "blue_example.hpp"

#include <sstream>

namespace examples::uefi_txt_measured_boot {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult result;

  // Reason 1: PCR / attestation integrity mismatch.
  if (!w.trust.attestation_pcr_ok || w.pcr_value_tampered) {
    result.pcr_mismatch = true;
    result.reasons.emplace_back("PCR mismatch or pcr_value_tampered");
  }

  // Reason 2: secure launch / measured boot not active.
  if (!w.trust.secure_launch) {
    result.event_log_tampered = true;
    result.reasons.emplace_back("secure_launch disabled");
  }

  // Reason 3: measured launch hide scar.
  if (w.trust.measured_launch_hidden) {
    result.reasons.emplace_back("measured_launch_hidden scar");
  }

  // Reason 4: attestation claims valid while PCR check failed.
  if (w.trust.attestation_valid && !w.trust.attestation_pcr_ok) {
    result.reasons.emplace_back("attestation_valid despite PCR failure");
  }

  // Reason 5: dual-boot / alternate PCR profile with measured-boot scars.
  if (w.trust.dual_boot_profile ||
      w.trust.boot_pcr_profile != "known_good") {
    result.reasons.emplace_back("dual_boot_profile or non-baseline PCR profile");
  }

  // Reason 6: sparse / scrubbed event log residual.
  if (w.tpm_event_log_entries.size() > 0 && w.tpm_event_log_entries.size() < 4) {
    result.missing_entries = 4 - static_cast<int>(w.tpm_event_log_entries.size());
    result.reasons.emplace_back("sparse tpm_event_log_entries residual");
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detection_count = result.signals;
  result.detected = result.signals >= 2;

  std::ostringstream oss;
  if (result.signals == 0) {
    oss << "TPM event log integrity verified";
  } else {
    oss << "uefi_txt_measured_boot blue signals=" << result.signals;
    for (const auto& reason : result.reasons) {
      oss << " | " << reason;
    }
  }
  result.detail = oss.str();
  return result;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  auto result = detect(w);
  if (result.detected) {
    w.note("Blue: TPM event log integrity check FAILED");
    w.note("Blue: failing remote attestation — component flagged");
    w.trust.attestation_valid = false;
    w.trust.measured_launch_hidden = false;
    w.pcr_value_tampered = false;
    w.ranked_access_denied = true;
    w.note("flag:attestation_failure_pending_ban");
    result.mitigated = true;
    result.detail += " | mitigated";
  }
  return result;
}

}  // namespace examples::uefi_txt_measured_boot
