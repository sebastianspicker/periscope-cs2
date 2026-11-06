// evasion_t3_advanced.cpp — multi-step T3 HV stealth orchestration on sim::World.
// max_hv_stealth: Wave 10 host-trust residuals after HV+bridge+entities.
// deep_hv_stealth: Wave 10 + Wave 11 nested/EPT/MSR/time/Hyper-V residuals.

#include "t3_red/evasion_t3_advanced.hpp"

#include <sstream>

namespace t3_red {

EvasionT3Advanced::EvasionT3Advanced(sim::World& world) : radar_(world) {}

// Wave 10 path: clear trust → personal HV → bridge → attach → base stealth →
// SMM channel, TPM spoof, EPT side-channel, VM-exit keylog, CI/FeatureControl,
// InfinityHook, VTL1 bypass → entity pull. Distinct from deep (no Wave 11).
HvRadarReport EvasionT3Advanced::max_hv_stealth(const std::string& vendor) {
  auto rep = radar_.run_full_loop(vendor, /*stealth=*/true);
  if (!rep.hv_started || !rep.entities_ok) {
    return radar_.last_report();
  }

  // Ordered Wave 10 residual plant — each mutates World + report fields.
  radar_.plant_smm_read_channel();
  radar_.trigger_smm_read();
  radar_.spoof_tpm_measurements(7);
  radar_.enable_ept_sidechannel_evasion();
  radar_.evade_ac_scan();
  radar_.enable_vmexit_keylog();
  radar_.enable_ci_options_spoof();
  radar_.enable_feature_control_spoof();
  radar_.enable_infinity_hook();
  radar_.enable_vtl1_enclave_bypass();

  // Keep read activity real after residual plant.
  radar_.pull_entities();

  HvRadarReport out = radar_.last_report();
  std::ostringstream oss;
  oss << "max_hv_stealth entities=" << out.entity_count
      << " smm_reads=" << out.smm_read_ops
      << " pcr=7"
      << " ept_evasions=" << out.ept_ac_scan_evasions
      << " keys=" << out.vmexit_keys_captured
      << " vendor=" << vendor;
  out.detail = oss.str();
  return out;
}

// Wave 10 + Wave 11: nested EPT shadow, MSR hooks, guest time dilation,
// Hyper-V enlightenment exploit layered on the full stealth loop.
HvRadarReport EvasionT3Advanced::deep_hv_stealth(const std::string& vendor) {
  auto rep = radar_.run_full_stealth_loop(vendor);
  if (!rep.hv_started) {
    return rep;
  }

  // Explicit Wave 11 re-assert with fixed lab parameters (idempotent scars).
  radar_.enable_ept_nested_hide(3);
  radar_.enable_msr_hooking("lstar,sysenter");
  radar_.enable_guest_time_dilation(0.5);
  radar_.enable_hyperv_enlightenment_exploit(0x4001);
  radar_.pull_entities();

  HvRadarReport out = radar_.last_report();
  std::ostringstream oss;
  oss << "deep_hv_stealth entities=" << out.entity_count
      << " ept_shadow=" << out.ept_shadow_levels
      << " msr=" << (out.msr_hooking ? 1 : 0)
      << " dilation=" << out.time_dilation_factor
      << " hyperv_call=0x4001"
      << " vendor=" << vendor;
  out.detail = oss.str();
  return out;
}

}  // namespace t3_red
