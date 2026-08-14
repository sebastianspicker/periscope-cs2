#include "red_example.hpp"

namespace examples::uefi_secureboot_bypass {

RedResult Red::apply(sim::World& w) noexcept {
  RedResult r;

  // Phase 1: disable Secure Boot trust scar.
  const bool was_enabled = w.trust.secure_boot;
  w.trust.secure_boot = false;
  r.secure_boot_disabled = was_enabled && !w.trust.secure_boot;
  w.note("secure_boot_variable_modified=true");
  ++r.steps;

  // Phase 2: unexpected EFI boot entry residual (custom recovery path).
  w.trust.unexpected_efi_entry = true;
  w.trust.efi_entry_name = "LabCustomDbLoader";
  w.note("uefi_variable_name=db custom entry residual");
  r.custom_db_installed = true;
  ++r.steps;

  // Phase 3: dual-boot / alternate PCR profile + DSE off for unsigned drivers.
  w.trust.dual_boot_profile = true;
  w.trust.boot_pcr_profile = "custom_db_path";
  w.trust.dse_enforced = false;
  w.trust.test_signing = true;
  r.unsigned_driver_allowed = true;
  ++r.steps;

  // Phase 4: plant unsigned driver scar + weaken attestation chain.
  w.module_is_signed = false;
  w.load_driver(sim::Driver{"lab-unsigned.sys", "sha-unsigned", "", false, false,
                            false, false, true});
  w.trust.attestation_pcr_ok = false;
  ++r.steps;

  r.achieved = r.steps >= 2 && !w.trust.secure_boot &&
               w.trust.unexpected_efi_entry && !w.trust.dse_enforced;
  r.detail = "uefi_secureboot_bypass red steps=" + std::to_string(r.steps) +
             " secure_boot=0 efi_entry=1 dual_boot=1 dse=0";
  w.note(r.detail);
  return r;
}

}  // namespace examples::uefi_secureboot_bypass
