// hv_radar.cpp — T3 red HV path using World.trust posture flags.
// Simulated personal HV / attestation fields

#include "t3_red/hv_radar.hpp"

#include "t0_red/entity_pipeline.hpp"

#include <sstream>

namespace t3_red {

// HvRadar::HvRadar: T3 red HV radar orchestrator.
HvRadar::HvRadar(sim::World& world, std::string ui)
    : world_(world), ui_name_(std::move(ui)) {
  ui_pid_ = world_.spawn(ui_name_);
  game_pid_ = world_.game_pid();
}

// HvRadar::force_disable_vbs_for_lab: Lab helper: clear VBS so personal HV can start.
void HvRadar::force_disable_vbs_for_lab() {
  world_.trust.vbs = false;
  world_.trust.hvci = false;
  world_.trust.hvci_enabled = false;
  world_.trust.secure_boot = false;  // often collateral
  last_.trust_cleared = true;
  world_.note("user disabled VBS/HVCI per cheat guide");
}

// HvRadar::try_hv: Attempt personal HV start + hv_read path.
bool HvRadar::try_hv(const std::string& vendor) {
  if (!game_pid_) {
    game_pid_ = world_.game_pid();
  }
  if (auto* g = world_.proc(game_pid_)) {
    if (g->memory.size() < 0x30) {
      world_.plant_lab_entities(game_pid_);
    }
  }
  auto st = backend_.simulate_hv_init(world_, vendor);
  hv_ = (st == ac::Status::Ok) && world_.trust.personal_hv_active;
  last_.hv_started = hv_;
  last_.vendor = vendor;
  return hv_;
}

// HvRadar::open_bridge: Open bridge surface for HV-backed reads.
bool HvRadar::open_bridge() {
  auto br = bridge_surf_.open_on_world(world_, "hvcomm.sys", bridge_);
  bridge_open_ = br.open;
  last_.bridge_open = bridge_open_;
  return bridge_open_;
}

// HvRadar::attach_best: Try HV then fallback attach path.
AttachResult HvRadar::attach_best() {
  if (hv_ && bridge_open_) {
    auto st = backend_.attach_world(world_, ui_pid_, game_pid_);
    if (st == ac::Status::Ok) {
      active_ = BackendKind::Hv;
      return {true, BackendKind::Hv, "personal HV + bridge"};
    }
  }
  if (hv_) {
    auto st = backend_.attach_world(world_, ui_pid_, game_pid_);
    if (st == ac::Status::Ok) {
      active_ = BackendKind::Hv;
      return {true, BackendKind::Hv, "HV without explicit bridge (lab)"};
    }
  }
  active_ = BackendKind::UsermodeRpm;
  return {false, BackendKind::UsermodeRpm,
          "HV unavailable — fall back externally"};
}

// HvRadar::apply_stealth: Apply EPT/stealth residual flags.
void HvRadar::apply_stealth(bool ept_hide, bool timing_spoof, bool fail_attest) {
  if (ept_hide) {
    world_.trust.ept_hide_ac_pages = true;
    world_.trust.guest_ac_view_clean = true;
    world_.trust.secure_kernel_view_dirty = true;
    last_.ept_hide = true;
  }
  if (timing_spoof) {
    world_.trust.timing_spoofed = true;
    // Keep inflated latency inconsistent with spoof claim for multi-invariant.
    if (world_.trust.baseline_latency_ns <= 0) {
      world_.trust.baseline_latency_ns = 100;
    }
    world_.trust.cpuid_latency_ns = world_.trust.baseline_latency_ns * 8;
    last_.timing_spoof = true;
  }
  if (fail_attest) {
    world_.trust.attestation_valid = false;
    world_.trust.attestation_pcr_ok = false;
    last_.attest_fail = true;
  }
  world_.note("t3 HvRadar apply_stealth ept=" + std::to_string(ept_hide) +
              " timing=" + std::to_string(timing_spoof) +
              " attest_fail=" + std::to_string(fail_attest));
}

void HvRadar::plant_smm_read_channel() {
  const bool reuses_acpi_pm_timer = world_.acpi_pm_read_active;
  world_.acpi_pm_read_active = true;
  world_.smm_read_channel_planted = true;
  last_.smm_read_channel = true;
  world_.note(reuses_acpi_pm_timer
                  ? "t3 HvRadar SMM channel reused ACPI PM timer"
                  : "t3 HvRadar SMM channel created lab SMI vector");
}

void HvRadar::trigger_smm_read() {
  if (!world_.smm_read_channel_planted) {
    return;
  }
  ++world_.acpi_smi_trigger_count;
  ++world_.smm_read_ops;
  last_.smm_read_ops = world_.smm_read_ops;
  world_.note("t3 HvRadar SMM memory read triggered");
}

void HvRadar::spoof_tpm_measurements(int pcr_bank) {
  world_.tpm_measurement_spoofed = true;
  world_.spoofed_pcr_bank = pcr_bank;
  world_.trust.attestation_valid = false;
  world_.trust.attestation_pcr_ok = false;
  last_.tpm_measurement_spoof = true;
  last_.attest_fail = true;
  world_.note("t3 HvRadar TPM PCR bank " + std::to_string(pcr_bank) +
              " spoofed; attestation invalidated");
}

void HvRadar::enable_ept_sidechannel_evasion() {
  world_.ept_sidechannel_active = true;
  world_.invept_tlb_flush_used = true;
  last_.ept_sidechannel = true;
  last_.invept_tlb_flush = true;
  world_.note("t3 HvRadar EPT scan-window side channel enabled with INVEPT");
}

void HvRadar::evade_ac_scan() {
  if (!world_.ept_sidechannel_active) {
    return;
  }
  ++world_.ept_ac_scan_evasions;
  last_.ept_ac_scan_evasions = world_.ept_ac_scan_evasions;
  world_.note("t3 HvRadar evaded an AC scan window");
}

void HvRadar::enable_vmexit_keylog() {
  world_.vmexit_keylog_active = true;
  last_.vmexit_keylog = true;
  simulate_key_capture(1);
  world_.note("t3 HvRadar VM-exit keyboard IRQ capture enabled");
}

void HvRadar::simulate_key_capture(int count) {
  if (!world_.vmexit_keylog_active || count <= 0) {
    return;
  }
  world_.vmexit_keys_captured += count;
  last_.vmexit_keys_captured = world_.vmexit_keys_captured;
}

void HvRadar::enable_ci_options_spoof() {
  world_.ci_options_disabled = true;
  world_.trust.dse_enforced = false;
  last_.ci_options_spoof = true;
  world_.note("t3 HvRadar CI options disabled in lab state");
}

void HvRadar::enable_feature_control_spoof() {
  world_.feature_control_spoofed = true;
  last_.feature_control_spoof = true;
  world_.note("t3 HvRadar IA32_FEATURE_CONTROL spoofed");
}

void HvRadar::enable_vtl1_enclave_bypass() {
  world_.vtl1_enclave_present = false;
  last_.vtl1_enclave_bypass = true;
  world_.note("t3 HvRadar VTL1 enclave presence bypassed");
}

void HvRadar::enable_infinity_hook() {
  world_.infinity_hook_residual = true;
  last_.infinity_hook = true;
  world_.note("t3 HvRadar InfinityHook residual planted");
}

void HvRadar::enable_ept_nested_hide(int levels) {
  world_.ept_nested_hide_active = true;
  world_.ept_shadow_levels = levels;
  last_.ept_nested_hide = true;
  last_.ept_shadow_levels = levels;
  world_.note("t3 HvRadar shadow EPT hide enabled levels=" +
              std::to_string(levels));
}

void HvRadar::enable_msr_hooking(const std::string& msr_list) {
  world_.msr_hooking_active = true;
  world_.hooked_msr_list = msr_list;
  last_.msr_hooking = true;
  world_.note("t3 HvRadar MSR hooks enabled: " + msr_list);
}

void HvRadar::enable_guest_time_dilation(double factor) {
  world_.guest_time_dilation_active = true;
  world_.time_dilation_factor = factor;
  last_.guest_time_dilation = true;
  last_.time_dilation_factor = factor;
  world_.note("t3 HvRadar guest time dilation factor=" +
              std::to_string(factor));
}

void HvRadar::enable_hyperv_enlightenment_exploit(int call_number) {
  world_.hyperv_enlightenment_exploit = true;
  world_.hyperv_enlightenment_call = call_number;
  last_.hyperv_enlightenment = true;
  world_.note("t3 HvRadar Hyper-V enlightenment exploit call=" +
              std::to_string(call_number));
}

// HvRadar::has_game_handle: Return/query has game handle for this lab unit.
bool HvRadar::has_game_handle() const {
  for (const auto& h : world_.handles_to(game_pid_, true)) {
    if (h.owner_pid == ui_pid_ && sim::has(h.access, sim::AccessMask::VmRead)) {
      return true;
    }
  }
  return false;
}

// HvRadar::pull_entities: Refresh entity pipeline and cache living set for radar UI.
bool HvRadar::pull_entities() {
  entities_.clear();
  auto* g = world_.proc(game_pid_);
  if (!g) {
    return false;
  }

  if (backend_.is_attached()) {
    t0_red::EntityPipeline pipe(backend_);
    if (pipe.refresh(g->base) != ac::Status::Ok) {
      last_.entities_ok = false;
      return false;
    }
    entities_ = pipe.entities();
  } else {
    // Direct hv_read path
    std::vector<std::uint8_t> count_b;
    if (!world_.hv_read(ui_pid_, game_pid_, g->base, 4, count_b) ||
        count_b.size() < 4) {
      return false;
    }
    std::uint32_t count = 0;
    std::memcpy(count_b.data() ? count_b.data() : count_b.data(), count_b.data(),
                4);
    // safer memcpy
    std::memcpy(&count, count_b.data(), 4);
    struct Ent {
      float x, y, z;
      std::uint8_t team, alive, pad[2];
    };
    for (std::uint32_t i = 0; i < count && i < 32; ++i) {
      std::vector<std::uint8_t> eb;
      const auto addr = g->base + 0x10 + i * sizeof(Ent);
      if (!world_.hv_read(ui_pid_, game_pid_, addr, sizeof(Ent), eb)) {
        return false;
      }
      Ent e{};
      std::memcpy(&e, eb.data(), sizeof(e));
      entities_.push_back(
          ac::EntitySnapshot{i, {e.x, e.y, e.z}, e.team, e.alive != 0});
    }
  }

  last_.entities_ok = !entities_.empty();
  last_.entity_count = static_cast<int>(entities_.size());
  last_.read_ops = backend_.read_ops();
  last_.bytes_read = backend_.bytes_read_total();
  last_.no_game_handle = !has_game_handle();
  last_.active = active_;
  return last_.entities_ok;
}

// HvRadar::run_full_loop: Red attach/read then blue sensors/mitigate on one World tick.
HvRadarReport HvRadar::run_full_loop(const std::string& vendor, bool stealth) {
  last_ = {};
  force_disable_vbs_for_lab();
  if (!try_hv(vendor)) {
    last_.detail = "hv_start_failed";
    return last_;
  }
  open_bridge();
  auto ar = attach_best();
  if (!ar.ok) {
    last_.detail = ar.detail;
    return last_;
  }
  if (!pull_entities()) {
    last_.detail = "pull_failed";
    return last_;
  }
  if (stealth) {
    apply_stealth(true, true, true);
  }
  std::ostringstream oss;
  oss << "full_loop entities=" << last_.entity_count
      << " no_handle=" << (last_.no_game_handle ? 1 : 0)
      << " hv=" << (last_.hv_started ? 1 : 0)
      << " bridge=" << (last_.bridge_open ? 1 : 0)
      << " ept=" << (last_.ept_hide ? 1 : 0)
      << " timing=" << (last_.timing_spoof ? 1 : 0)
      << " vendor=" << last_.vendor;
  last_.detail = oss.str();
  world_.note("t3 HvRadar " + last_.detail);
  return last_;
}

HvRadarReport HvRadar::run_full_stealth_loop(const std::string& vendor) {
  last_ = {};
  force_disable_vbs_for_lab();
  if (!try_hv(vendor)) {
    last_.detail = "hv_start_failed";
    return last_;
  }
  open_bridge();
  const auto attach = attach_best();
  if (!attach.ok) {
    last_.detail = attach.detail;
    return last_;
  }

  apply_stealth(true, true, true);
  plant_smm_read_channel();
  trigger_smm_read();
  spoof_tpm_measurements(7);
  enable_ept_sidechannel_evasion();
  evade_ac_scan();
  enable_vmexit_keylog();
  enable_ci_options_spoof();
  enable_feature_control_spoof();
  enable_infinity_hook();
  enable_vtl1_enclave_bypass();
  enable_ept_nested_hide(3);
  enable_msr_hooking();
  enable_guest_time_dilation();
  enable_hyperv_enlightenment_exploit();

  if (!pull_entities()) {
    last_.detail = "pull_failed";
    return last_;
  }
  std::ostringstream oss;
  oss << "full_stealth_loop entities=" << last_.entity_count
      << " smm_reads=" << last_.smm_read_ops
      << " pcr=" << world_.spoofed_pcr_bank
      << " ept_evasions=" << last_.ept_ac_scan_evasions
      << " keys=" << last_.vmexit_keys_captured
      << " vendor=" << last_.vendor;
  last_.detail = oss.str();
  world_.note("t3 HvRadar " + last_.detail);
  return last_;
}

}  // namespace t3_red
