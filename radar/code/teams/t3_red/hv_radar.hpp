#pragma once

// Full T3 educational client: trust-off → personal HV → bridge → HV reads.
// Optional stealth: EPT hide, timing spoof, CR3 note. Fallback markers for T2/T1.

#include "ac/types.hpp"
#include "sim/world.hpp"
#include "t3_red/bridge_surface.hpp"
#include "t3_red/hv_backend.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t3_red {

enum class BackendKind { Hv, KernelIoctl, UsermodeRpm };

// Aggregate outcome fields for `AttachResult` (lab narrative / tests).
struct AttachResult {
  bool ok = false;
  BackendKind kind = BackendKind::UsermodeRpm;
  std::string detail;
};

// Aggregate outcome fields for `HvRadarReport` (lab narrative / tests).
struct HvRadarReport {
  bool trust_cleared = false;
  bool hv_started = false;
  bool bridge_open = false;
  bool entities_ok = false;
  bool no_game_handle = false;
  bool ept_hide = false;
  bool timing_spoof = false;
  bool attest_fail = false;
  bool smm_read_channel = false;
  bool tpm_measurement_spoof = false;
  bool ept_sidechannel = false;
  bool invept_tlb_flush = false;
  bool vmexit_keylog = false;
  bool ci_options_spoof = false;
  bool feature_control_spoof = false;
  bool infinity_hook = false;
  bool vtl1_enclave_bypass = false;
  bool ept_nested_hide = false;
  int ept_shadow_levels = 0;
  bool msr_hooking = false;
  bool guest_time_dilation = false;
  double time_dilation_factor = 1.0;
  bool hyperv_enlightenment = false;
  int entity_count = 0;
  int read_ops = 0;
  int smm_read_ops = 0;
  int ept_ac_scan_evasions = 0;
  int vmexit_keys_captured = 0;
  std::uint64_t bytes_read = 0;
  BackendKind active = BackendKind::UsermodeRpm;
  std::string vendor;
  std::string detail;
};

// Multi-step educational red client `HvRadar` — plants lab scars on World only.
class HvRadar {
 public:
  explicit HvRadar(sim::World& world, std::string ui = "hv-radar.exe");

  void force_disable_vbs_for_lab();
  bool try_hv(const std::string& vendor = "ACLABHV");
  bool open_bridge();
  AttachResult attach_best();
  // Read lab entity table through the attached backend into snapshots/blips.
  bool pull_entities();

  /// Apply common T3 stealth residuals after HV is up.
  void apply_stealth(bool ept_hide = true, bool timing_spoof = true,
                     bool fail_attest = true);

  // Advanced T3 lab scars. These only mutate sim::World state.
  void plant_smm_read_channel();
  void trigger_smm_read();
  void spoof_tpm_measurements(int pcr_bank);
  void enable_ept_sidechannel_evasion();
  void evade_ac_scan();
  void enable_vmexit_keylog();
  void simulate_key_capture(int count);
  void enable_ci_options_spoof();
  void enable_feature_control_spoof();
  void enable_vtl1_enclave_bypass();
  void enable_infinity_hook();
  void enable_ept_nested_hide(int levels = 3);
  void enable_msr_hooking(const std::string& msr_list = "lstar,sysenter");
  void enable_guest_time_dilation(double factor = 0.5);
  void enable_hyperv_enlightenment_exploit(int call_number = 0x4001);

  /// Full loop: clear trust → HV → bridge → attach → pull → optional stealth.
  HvRadarReport run_full_loop(const std::string& vendor = "ACLABHV",
                              bool stealth = true);

  /// Full advanced loop: apply every T3 lab stealth residual before reading.
  HvRadarReport run_full_stealth_loop(const std::string& vendor = "ACLABHV");

  bool has_game_handle() const;

  std::uint32_t ui_pid() const { return ui_pid_; }
  std::uint32_t game_pid() const { return game_pid_; }
  bool bridge_open() const { return bridge_open_; }
  bool hv_active() const { return hv_; }
  BackendKind active() const { return active_; }
  const std::vector<ac::EntitySnapshot>& entities() const { return entities_; }
  const std::string& bridge_name() const { return bridge_; }
  const HvRadarReport& last_report() const { return last_; }
  HvReadBackend& backend() { return backend_; }
  BridgeSurface& bridge_surface() { return bridge_surf_; }

 private:
  sim::World& world_;
  std::string ui_name_;
  std::uint32_t ui_pid_ = 0;
  std::uint32_t game_pid_ = 0;
  std::string bridge_ = "\\\\.\\AcLabHvComm";
  bool bridge_open_ = false;
  bool hv_ = false;
  BackendKind active_ = BackendKind::UsermodeRpm;
  std::vector<ac::EntitySnapshot> entities_;
  HvReadBackend backend_;
  BridgeSurface bridge_surf_;
  HvRadarReport last_{};
};

}  // namespace t3_red
