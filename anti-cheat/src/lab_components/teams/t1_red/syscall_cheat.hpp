#pragma once

// Full T1 soft-external client on sim::World:
// staged loader + encrypted offsets + syscall-path RPM + entity pull.

#include "ac/types.hpp"
#include "sim/world.hpp"
#include "t1_red/crypto_offsets.hpp"
#include "t1_red/staged_loader.hpp"
#include "t1_red/syscall_backend.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t1_red {

// Aggregate outcome fields for `SyscallCheatReport` (lab narrative / tests).
struct SyscallCheatReport {
  bool staged = false;
  bool offsets_ok = false;
  bool attached = false;
  bool entities_ok = false;
  bool via_syscall = false;
  int entity_count = 0;
  int syscall_handles = 0;
  int read_ops = 0;
  std::uint64_t bytes_read = 0;
  std::uint32_t stub_pid = 0;
  std::uint32_t reader_pid = 0;
  bool private_rx = false;
  bool stack_spoof = false;
  bool etw_blind = false;
  bool heavens_gate = false;
  std::uint32_t heavens_gate_stub_pid = 0;
  bool enhanced_stack_spoof = false;
  int spoofed_call_depth = 0;
  bool dynamic_ssn_resolved = false;
  int resolved_ssn_windows_build = 0;
  bool ntdll_hooks_detected = false;
  int hooks_evaded_count = 0;
  bool used_clean_ntdll_copy = false;
  bool sedebug_enabled = false;
  bool threshold_evasion = false;
  bool thread_hidden_from_debugger = false;
  bool peb_being_debugged_spoofed = false;
  bool rop_chain_syscall = false;
  int rop_gadget_count = 0;
  bool hw_breakpoint_evasion = false;
  bool veh_cf_patched = false;
  int veh_handlers_modified = 0;
  bool dynamic_import_resolution = false;
  int dynamic_import_count = 0;
  std::string detail;
};

// Multi-step educational red client `SyscallCheat` — plants lab scars on World only.
class SyscallCheat {
 public:
  explicit SyscallCheat(sim::World& world,
                        std::string name = "soft-radar.exe");

  /// Multi-step stage: stub → auth → payload (optional before attach).
  bool stage(const char* token = "lab-token",
             const std::vector<std::uint8_t>& payload = {0x90, 0xC3});

  /// Load encrypted offsets (entity table base offsets for pedagogy).
  bool load_offsets(std::uint8_t key = 0x3C);

  /// Attach via syscall-shaped OpenProcess (leaves via_syscall_path handle).
  bool attach_via_syscall();

  /// Pull entity table through SyscallBackend / World RPM.
  bool pull_entities();

  /// Optional: mark stack spoof / ETW blind scars (evasion surface).
  void apply_read_evasions(bool stack_spoof = true, bool etw_blind = false);

  /// Spawn a simulated WOW64 stub and record its Heaven's Gate transition.
  bool enable_heavens_gate();

  /// Forge a simulated multi-frame return stack for read-side operations.
  bool enable_enhanced_stack_spoof(int depth);

  /// Resolve syscall numbers for an exact target Windows build in the lab.
  bool resolve_dynamic_ssns(int windows_build);

  /// Detect simulated AC ntdll hooks and map a clean simulated copy if found.
  bool detect_and_evade_ntdll_hooks();

  /// Enable the simulated SeDebugPrivilege scar for this red process.
  bool enable_sedebug();

  /// Restrict reader activity to active matches and apply debugger evasions.
  bool enable_threshold_evasion();

  /// Execute simulated syscalls through a ROP gadget chain.
  bool enable_rop_chain_syscall(int gadget_count);

  /// Clear simulated debug registers before reader activity.
  bool enable_hw_breakpoint_evasion();

  /// Patch simulated VEH control-flow handlers.
  bool enable_veh_cf_patch(int count);

  /// Resolve simulated imports dynamically rather than through the import table.
  bool enable_dynamic_import_resolution(int count);

  /// Full loop: stage → offsets → attach → pull → optional evasions.
  SyscallCheatReport run_full_loop(bool with_staging = true,
                                   bool with_evasions = false);

  /// Full advanced lab sequence using every T1 evasion scar surface.
  SyscallCheatReport run_full_stealth_loop();

  void detach();

  // Legacy helpers used by existing demos/tests.
  void stage_payload_lab();
  bool has_private_rx() const { return private_rx_; }

  std::uint32_t pid() const { return pid_; }
  std::uint32_t game_pid() const { return game_pid_; }
  bool attached() const { return attached_; }
  const std::vector<ac::EntitySnapshot>& entities() const { return entities_; }
  SyscallBackend& backend() { return backend_; }
  const SyscallCheatReport& last_report() const { return last_; }
  StagedLoader& loader() { return loader_; }
  sim::World& world() { return world_; }

 private:
  int count_syscall_handles_() const;

  sim::World& world_;
  std::string name_;
  std::uint32_t pid_ = 0;
  std::uint32_t game_pid_ = 0;
  bool attached_ = false;
  bool private_rx_ = false;
  SyscallBackend backend_;
  StagedLoader loader_;
  CryptoOffsets offsets_;
  std::vector<ac::EntitySnapshot> entities_;
  SyscallCheatReport last_{};
};

// Lab type `HookBlindness` used by this educational unit.
struct HookBlindness {
  bool usermode_rpm_hooks_fired = false;
  bool handle_still_exists = true;
  const char* lesson =
      "Syscall path blinds API hooks; handle graph still sees the open.";
};

HookBlindness describe_hook_blindness();

}  // namespace t1_red
