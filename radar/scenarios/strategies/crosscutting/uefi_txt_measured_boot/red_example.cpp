#include "red_example.hpp"

namespace examples::uefi_txt_measured_boot {

void Red::apply(sim::World& w) noexcept {
  (void)apply_detailed(w);
}

RedResult Red::apply_detailed(sim::World& w) noexcept {
  RedResult r;

  // Phase 1: claim attestation still valid while preparing log tamper.
  w.note("Red: reading TPM event log from UEFI config table");
  w.trust.attestation_valid = true;  // still claims to pass
  ++r.steps;

  // Phase 2: disable secure launch / measured boot path.
  w.trust.secure_launch = false;
  w.trust.measured_launch_hidden = true;
  w.note("Red: removing early-load measurement entries");
  ++r.steps;

  // Phase 3: PCR reconstruction fails; event log residual scrubbed.
  w.trust.attestation_pcr_ok = false;
  w.pcr_value_tampered = true;
  r.pcr_value_spoofed = true;
  r.event_log_tampered = true;
  w.tpm_event_log_entries.clear();
  w.tpm_event_log_entries.push_back(
      {0, 0x80000001, "deadbeef", "EV_EFI_VARIABLE_BOOT (scrubbed)"});
  w.tpm_event_log_entries.push_back(
      {17, 0x00000008, "cafebabe", "EV_S_CRTM_CONTENTS (gap)"});
  r.events_removed = 3;
  w.note("Red: spoofed PCR 17-18 values to hide early load");
  ++r.steps;

  // Phase 4: dual-boot / alternate PCR profile scar (related trust residual).
  w.trust.dual_boot_profile = true;
  w.trust.boot_pcr_profile = "txt_hide_early";
  ++r.steps;

  r.achieved = r.steps >= 2 && !w.trust.attestation_pcr_ok &&
               !w.trust.secure_launch && w.trust.measured_launch_hidden;
  r.detail = "uefi_txt_measured_boot red steps=" + std::to_string(r.steps) +
             " pcr_ok=0 secure_launch=0 measured_hidden=1";
  w.note(r.detail);
  return r;
}

}  // namespace examples::uefi_txt_measured_boot
