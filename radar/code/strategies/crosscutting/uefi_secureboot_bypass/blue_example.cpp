#include "blue_example.hpp"

#include <sstream>

namespace examples::uefi_secureboot_bypass {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult r;

  // Reason 1: Secure Boot disabled.
  r.secure_boot_status_changed = !w.trust.secure_boot;
  if (r.secure_boot_status_changed) {
    r.reasons.emplace_back("secure_boot disabled");
  }

  // Reason 2: unexpected EFI boot entry residual.
  if (w.trust.unexpected_efi_entry || !w.trust.efi_entry_name.empty()) {
    r.uefi_variable_modified = true;
    r.reasons.emplace_back("unexpected_efi_entry or custom efi_entry_name");
  }

  // Reason 3: dual-boot / non-baseline PCR profile.
  if (w.trust.dual_boot_profile ||
      w.trust.boot_pcr_profile != "known_good") {
    r.reasons.emplace_back("dual_boot_profile or non-baseline boot_pcr_profile");
  }

  // Reason 4: DSE off / test-signing / unsigned module path.
  if (!w.trust.dse_enforced || w.trust.test_signing || !w.module_is_signed) {
    r.unsigned_driver_detected = true;
    r.reasons.emplace_back("DSE off, test_signing, or unsigned module");
  }

  // Reason 5: unsigned driver image loaded.
  for (const auto& d : w.drivers) {
    if (d.signer.empty() || d.name.find("unsigned") != std::string::npos) {
      r.unsigned_driver_detected = true;
      r.reasons.emplace_back("unsigned driver image present");
      break;
    }
  }

  // Reason 6: PCR attestation inconsistency with boot trust scars.
  if (!w.trust.attestation_pcr_ok && !w.trust.secure_boot) {
    r.reasons.emplace_back("attestation_pcr_ok false with secure_boot off");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detection_count = r.signals;
  r.detected = r.signals >= 2;

  std::ostringstream oss;
  oss << "uefi_secureboot_bypass blue signals=" << r.signals
      << " secure_boot_changed=" << (r.secure_boot_status_changed ? 1 : 0)
      << " unsigned_driver=" << (r.unsigned_driver_detected ? 1 : 0);
  r.detail = oss.str();
  return r;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  BlueResult r = detect(w);
  if (r.detected) {
    w.trust.secure_boot = true;
    w.trust.dse_enforced = true;
    w.trust.test_signing = false;
    w.trust.unexpected_efi_entry = false;
    w.trust.efi_entry_name.clear();
    w.trust.dual_boot_profile = false;
    w.trust.boot_pcr_profile = "known_good";
    w.trust.attestation_pcr_ok = true;
    w.ranked_access_denied = true;
    w.note("Blue: re-enabled Secure Boot via firmware callback");
    w.note("Blue: TPM PCR mismatch logged — attestation re-baselined");
    r.mitigated = true;
    r.detail += " | mitigated";
  }
  w.note(r.detail);
  return r;
}

}  // namespace examples::uefi_secureboot_bypass
