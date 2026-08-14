// syscall_cheat.cpp — T1 red syscall-soft path (indirect/stub) over lab World.
// Plants educational residuals.

#include "t1_red/syscall_cheat.hpp"

#include "t0_red/entity_pipeline.hpp"

#include <cstring>
#include <sstream>
#include <unordered_map>

namespace t1_red {

// SyscallCheat::SyscallCheat: T1 red client using syscall-soft attach path.
SyscallCheat::SyscallCheat(sim::World& world, std::string name)
    : world_(world), name_(std::move(name)) {
  pid_ = world_.spawn(name_);
}

// SyscallCheat::stage: Plant staging residuals (private RX / stub) on World.
bool SyscallCheat::stage(const char* token,
                         const std::vector<std::uint8_t>& payload) {
  auto rep = loader_.run_on_world(world_, token, payload, name_);
  private_rx_ = rep.private_rx;
  // Prefer payload child as reader if staging produced one.
  if (rep.payload_pid != 0) {
    pid_ = rep.payload_pid;
  }
  last_.staged = rep.state == StageState::PayloadMapped;
  last_.stub_pid = rep.stub_pid;
  last_.reader_pid = pid_;
  last_.private_rx = private_rx_;
  last_.detail = rep.detail;
  return last_.staged;
}

// SyscallCheat::load_offsets: Load lab offset blob into the cheat client.
bool SyscallCheat::load_offsets(std::uint8_t key) {
  std::unordered_map<std::string, std::uint64_t> m = {
      {"entity_base", 0x00},
      {"entity_off", 0x10},
      {"count_off", 0x00},
  };
  auto sealed = CryptoOffsets::seal(m, key);
  auto st = offsets_.open(sealed, key);
  last_.offsets_ok = (st == ac::Status::Ok);
  if (last_.offsets_ok) {
    // Optional C2 scar.
    world_.add_net(
        sim::NetFlow{pid_, "cdn-offsets.lab:443", true, false});
  }
  return last_.offsets_ok;
}

// SyscallCheat::attach_via_syscall: T1 attach path that avoids usermode OpenProcess hooks in sim.
bool SyscallCheat::attach_via_syscall() {
  if (last_.threshold_evasion && !world_.match_active) {
    last_.attached = false;
    last_.detail = "match_inactive_attach_deferred";
    world_.note("t1 SyscallCheat threshold_evasion deferred attach outside match");
    return false;
  }
  game_pid_ = world_.game_pid();
  if (!game_pid_) {
    last_.detail = "no_game";
    return false;
  }
  if (auto* g = world_.proc(game_pid_)) {
    if (g->memory.size() < 0x30) {
      world_.plant_lab_entities(game_pid_);
    }
  }

  const auto st = backend_.attach_world(world_, pid_, game_pid_);
  attached_ = (st == ac::Status::Ok);
  last_.attached = attached_;
  last_.via_syscall = attached_;
  last_.syscall_handles = count_syscall_handles_();
  last_.reader_pid = pid_;
  last_.detail = attached_ ? "attached_syscall_vm_read" : "attach_failed";
  return attached_;
}

// SyscallCheat::pull_entities: Refresh entity pipeline and cache living set for radar UI.
bool SyscallCheat::pull_entities() {
  entities_.clear();
  if (!attached_ || !backend_.is_attached()) {
    last_.entities_ok = false;
    return false;
  }
  auto* g = world_.proc(game_pid_);
  if (!g) {
    return false;
  }
  t0_red::EntityPipeline pipe(backend_);
  if (pipe.refresh(g->base) != ac::Status::Ok) {
    last_.entities_ok = false;
    last_.detail = "refresh_failed";
    return false;
  }
  entities_ = pipe.entities();
  last_.entities_ok = !entities_.empty();
  last_.entity_count = static_cast<int>(entities_.size());
  last_.read_ops = backend_.read_ops();
  last_.bytes_read = backend_.bytes_read_total();
  last_.syscall_handles = count_syscall_handles_();
  return last_.entities_ok;
}

// SyscallCheat::apply_read_evasions: Apply weak read-side evasions after attach.
void SyscallCheat::apply_read_evasions(bool stack_spoof, bool etw_blind) {
  if (stack_spoof) {
    world_.stack_spoof_on_read = true;
    last_.stack_spoof = true;
  }
  if (etw_blind) {
    world_.etw_enabled = false;
    last_.etw_blind = true;
  }
  world_.note("t1 SyscallCheat read_evasions stack_spoof=" +
              std::to_string(stack_spoof) +
              " etw_blind=" + std::to_string(etw_blind));
}

// SyscallCheat::enable_heavens_gate: Model a WOW64 stub crossing into 64-bit
// mode. This is a World-only scar; it never invokes an operating-system API.
bool SyscallCheat::enable_heavens_gate() {
  if (pid_ == 0 || !world_.proc(pid_)) {
    last_.detail = "heavens_gate_reader_missing";
    return false;
  }
  const auto stub = world_.spawn("wow64-heavens-gate-stub32.exe", false, false,
                                 pid_);
  if (stub == 0 || !world_.proc(stub)) {
    last_.detail = "heavens_gate_stub_spawn_failed";
    return false;
  }
  world_.heavens_gate_transition = true;
  world_.heavens_gate_stub_pid = stub;
  last_.heavens_gate = true;
  last_.heavens_gate_stub_pid = stub;
  world_.note("t1 SyscallCheat Heaven's Gate WOW64 stub=" +
              std::to_string(stub) + " transitioned to simulated 64-bit syscall mode");
  return true;
}

// SyscallCheat::enable_enhanced_stack_spoof: Record a forged multi-frame stack.
bool SyscallCheat::enable_enhanced_stack_spoof(int depth) {
  if (depth <= 0) {
    last_.detail = "invalid_spoof_depth";
    return false;
  }
  world_.stack_spoof_on_read = true;
  world_.enhanced_stack_spoof = true;
  world_.spoofed_call_depth = depth;
  last_.stack_spoof = true;
  last_.enhanced_stack_spoof = true;
  last_.spoofed_call_depth = depth;
  world_.note("t1 SyscallCheat enhanced_stack_spoof depth=" +
              std::to_string(depth));
  return true;
}

// SyscallCheat::resolve_dynamic_ssns: Simulate resolving SSNs for this build.
bool SyscallCheat::resolve_dynamic_ssns(int windows_build) {
  if (windows_build <= 0) {
    last_.detail = "invalid_windows_build";
    return false;
  }
  world_.dynamic_ssn_resolved = true;
  world_.resolved_ssn_windows_build = windows_build;
  last_.dynamic_ssn_resolved = true;
  last_.resolved_ssn_windows_build = windows_build;
  world_.note("t1 SyscallCheat dynamic_ssn_resolved windows_build=" +
              std::to_string(windows_build));
  return true;
}

// SyscallCheat::detect_and_evade_ntdll_hooks: AC module hook flags model the
// in-memory-versus-disk ntdll comparison performed by the simulated client.
bool SyscallCheat::detect_and_evade_ntdll_hooks() {
  int hooks_found = 0;
  for (const auto& [unused_pid, process] : world_.processes) {
    (void)unused_pid;
    if (!process.is_ac) {
      continue;
    }
    for (const auto& module : process.modules) {
      hooks_found += static_cast<int>(module.iat_hooked);
      hooks_found += static_cast<int>(module.eat_hooked);
      hooks_found += static_cast<int>(module.present_hooked);
    }
  }
  if (hooks_found == 0) {
    world_.note("t1 SyscallCheat ntdll_hook_scan found=0");
    return false;
  }
  world_.ntdll_hooks_detected = true;
  world_.hooks_evaded_count = hooks_found;
  world_.used_clean_ntdll_copy = true;
  last_.ntdll_hooks_detected = true;
  last_.hooks_evaded_count = hooks_found;
  last_.used_clean_ntdll_copy = true;
  world_.note("t1 SyscallCheat ntdll_hook_scan found=" +
              std::to_string(hooks_found) + " mapped_clean_copy=1");
  return true;
}

// SyscallCheat::enable_sedebug: Set only the World privilege scar.
bool SyscallCheat::enable_sedebug() {
  if (pid_ == 0 || !world_.proc(pid_)) {
    last_.detail = "sedebug_reader_missing";
    return false;
  }
  world_.sedebug_privilege = true;
  last_.sedebug_enabled = true;
  world_.note("t1 SyscallCheat SeDebugPrivilege enabled for simulated reader=" +
              std::to_string(pid_));
  return true;
}

// SyscallCheat::enable_threshold_evasion: Reader activity follows match state;
// the debugger flags are global World scars shared by the simulated process view.
bool SyscallCheat::enable_threshold_evasion() {
  auto* reader = world_.proc(pid_);
  if (!reader) {
    last_.detail = "threshold_reader_missing";
    return false;
  }
  reader->reader_active = world_.match_active;
  world_.thread_hide_from_debugger = true;
  world_.peb_being_debugged_spoofed = true;
  last_.threshold_evasion = true;
  last_.thread_hidden_from_debugger = true;
  last_.peb_being_debugged_spoofed = true;
  world_.note("t1 SyscallCheat threshold_evasion match_active=" +
              std::to_string(world_.match_active) + " thread_hide=1 peb_spoof=1");
  return true;
}

// SyscallCheat::enable_rop_chain_syscall: Record a World-only ROP syscall path.
bool SyscallCheat::enable_rop_chain_syscall(int gadget_count) {
  if (gadget_count <= 0) {
    last_.detail = "invalid_rop_gadget_count";
    return false;
  }
  world_.rop_chain_syscall_active = true;
  world_.rop_gadget_count = gadget_count;
  last_.rop_chain_syscall = true;
  last_.rop_gadget_count = gadget_count;
  world_.note("t1 SyscallCheat rop_chain_syscall gadgets=" +
              std::to_string(gadget_count));
  return true;
}

// SyscallCheat::enable_hw_breakpoint_evasion: Clear simulated debug registers.
bool SyscallCheat::enable_hw_breakpoint_evasion() {
  world_.hw_breakpoint_evasion_active = true;
  world_.debug_registers_cleared = true;
  last_.hw_breakpoint_evasion = true;
  world_.note("t1 SyscallCheat hw_breakpoint_evasion debug_registers_cleared=1");
  return true;
}

// SyscallCheat::enable_veh_cf_patch: Record a simulated VEH CFG bypass scar.
bool SyscallCheat::enable_veh_cf_patch(int count) {
  if (count <= 0) {
    last_.detail = "invalid_veh_handler_count";
    return false;
  }
  world_.veh_cf_patched = true;
  world_.veh_handlers_modified = count;
  last_.veh_cf_patched = true;
  last_.veh_handlers_modified = count;
  world_.note("t1 SyscallCheat veh_cf_patch handlers=" +
              std::to_string(count));
  return true;
}

// SyscallCheat::enable_dynamic_import_resolution: Record runtime import lookup.
bool SyscallCheat::enable_dynamic_import_resolution(int count) {
  if (count <= 0) {
    last_.detail = "invalid_dynamic_import_count";
    return false;
  }
  world_.dynamic_import_resolution = true;
  world_.dynamic_import_count = count;
  last_.dynamic_import_resolution = true;
  last_.dynamic_import_count = count;
  world_.note("t1 SyscallCheat dynamic_import_resolution imports=" +
              std::to_string(count));
  return true;
}

// SyscallCheat::run_full_loop: Red attach/read then blue sensors/mitigate on one World tick.
SyscallCheatReport SyscallCheat::run_full_loop(bool with_staging,
                                               bool with_evasions) {
  last_ = {};
  if (with_staging) {
    if (!stage()) {
      last_.detail = "stage_failed";
      return last_;
    }
  }
  load_offsets();
  if (!attach_via_syscall()) {
    return last_;
  }
  if (!pull_entities()) {
    return last_;
  }
  if (with_evasions) {
    apply_read_evasions(true, true);
  }
  last_.reader_pid = pid_;
  std::ostringstream oss;
  oss << "full_loop entities=" << last_.entity_count
      << " syscall_handles=" << last_.syscall_handles
      << " reads=" << last_.read_ops << " bytes=" << last_.bytes_read
      << " private_rx=" << (last_.private_rx ? 1 : 0)
      << " staged=" << (last_.staged ? 1 : 0);
  last_.detail = oss.str();
  world_.note("t1 SyscallCheat " + last_.detail);
  return last_;
}

// SyscallCheat::run_full_stealth_loop: Execute the advanced T1 lesson in one
// World-only sequence. Attachment remains deferred unless a match is active.
SyscallCheatReport SyscallCheat::run_full_stealth_loop() {
  last_ = {};
  if (!stage()) {
    last_.detail = "stage_failed";
    return last_;
  }
  if (!load_offsets() || !enable_heavens_gate() ||
      !enable_enhanced_stack_spoof(4) || !resolve_dynamic_ssns(22631)) {
    return last_;
  }
  detect_and_evade_ntdll_hooks();
  if (!enable_sedebug() || !enable_threshold_evasion()) {
    return last_;
  }
  apply_read_evasions(false, true);
  if (world_.match_active) {
    if (!attach_via_syscall() || !pull_entities()) {
      return last_;
    }
  }
  last_.reader_pid = pid_;
  std::ostringstream oss;
  oss << "full_stealth_loop match_active=" << (world_.match_active ? 1 : 0)
      << " gate_stub=" << last_.heavens_gate_stub_pid
      << " stack_depth=" << last_.spoofed_call_depth
      << " ssn_build=" << last_.resolved_ssn_windows_build
      << " hooks_evaded=" << last_.hooks_evaded_count
      << " etw_blind=" << (last_.etw_blind ? 1 : 0);
  last_.detail = oss.str();
  world_.note("t1 SyscallCheat " + last_.detail);
  return last_;
}

// SyscallCheat::detach: Clear attach state.
void SyscallCheat::detach() {
  backend_.detach();
  attached_ = false;
}

// SyscallCheat::stage_payload_lab: Plant staging residuals (private RX / short stub) on World.
void SyscallCheat::stage_payload_lab() {
  private_rx_ = true;
  loader_.authenticate_lab("lab-token");
  loader_.map_payload_lab({0x90, 0xC3});
  last_.private_rx = true;
  last_.staged = true;
}

// SyscallCheat::count_syscall_handles_: Count handles opened via syscall_path flag.
int SyscallCheat::count_syscall_handles_() const {
  int n = 0;
  if (!game_pid_) {
    return 0;
  }
  for (const auto& h : world_.handles_to(game_pid_, true)) {
    if (h.owner_pid == pid_ && sim::has(h.access, sim::AccessMask::VmRead) &&
        h.via_syscall_path) {
      ++n;
    }
  }
  return n;
}

// describe_hook_blindness: free function for this educational unit.
HookBlindness describe_hook_blindness() {
  // Pedagogy: indirect syscalls skip usermode RPM hooks but the handle scar remains.
  HookBlindness h;
  h.usermode_rpm_hooks_fired = false;
  h.handle_still_exists = true;
  // lesson default string lives on the struct definition.
  return h;
}

}  // namespace t1_red
