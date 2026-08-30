// hook_trap.cpp — T1 blue hook/syscall-aware monitor on sim World scars.
// Multi-reason detect for soft-edge delivery.

#include "t1_blue/hook_trap.hpp"

#include <sstream>

namespace t1_blue {

// UsermodeHookTrap::would_see_attach: Whether usermode hook trap observes the attach attempt.
bool UsermodeHookTrap::would_see_attach(const sim::Handle& h) const {
  // Only winapi path crosses hooked ntdll stubs in this lab model.
  return sim::has(h.access, sim::AccessMask::VmRead) && !h.via_syscall_path;
}

// UsermodeHookTrap::count_visible_opens: Count OpenProcess-visible attaches still on the hook trap.
int UsermodeHookTrap::count_visible_opens(const sim::World& w,
                                          std::uint32_t game_pid) const {
  int n = 0;
  for (const auto& h : w.handles_to(game_pid, true)) {
    const auto* p = w.proc(h.owner_pid);
    if (!p || p->is_game || p->is_ac) {
      continue;
    }
    if (would_see_attach(h)) {
      ++n;
    }
  }
  return n;
}

// UsermodeHookTrap::analyze: UsermodeHookTrap: scan/analyze World residuals for blue signals.
HookTrapReport UsermodeHookTrap::analyze(const sim::World& w,
                                         std::uint32_t game_pid) const {
  HookTrapReport r;
  for (const auto& h : w.handles_to(game_pid, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) {
      continue;
    }
    const auto* p = w.proc(h.owner_pid);
    if (!p || p->is_game || p->is_ac) {
      continue;
    }
    if (h.via_syscall_path) {
      ++r.syscall_opens_missed;
    } else {
      ++r.visible_winapi_opens;
    }
  }
  r.blind_to_syscall_red =
      r.syscall_opens_missed > 0 && r.visible_winapi_opens == 0;
  std::ostringstream oss;
  oss << "hook_trap winapi_vis=" << r.visible_winapi_opens
      << " syscall_missed=" << r.syscall_opens_missed
      << " blind=" << (r.blind_to_syscall_red ? 1 : 0);
  r.detail = oss.str();
  return r;
}

// HandleTruthMonitor::count_vm_read_handles: Count VmRead handles (truth path vs hook-blind path).
int HandleTruthMonitor::count_vm_read_handles(const sim::World& w,
                                              std::uint32_t game_pid) const {
  return static_cast<int>(analyze(w, game_pid, true).foreign_vm_read);
}

// HandleTruthMonitor::analyze: HandleTruthMonitor: scan/analyze World residuals for blue signals.
HandleTruthReport HandleTruthMonitor::analyze(const sim::World& w,
                                              std::uint32_t game_pid,
                                              bool include_hidden) const {
  HandleTruthReport r;
  for (const auto& h : w.handles_to(game_pid, include_hidden)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) {
      continue;
    }
    const auto* p = w.proc(h.owner_pid);
    if (!p || p->is_game || p->is_ac) {
      continue;
    }
    ++r.foreign_vm_read;
    if (h.via_syscall_path) {
      ++r.syscall_path;
    } else {
      ++r.winapi_path;
    }
    if (h.hidden_during_enum) {
      ++r.hidden;
    }
    r.owners.push_back(p->name + "#" + std::to_string(h.owner_pid) +
                       (h.via_syscall_path ? ":syscall" : ":winapi"));
  }
  std::ostringstream oss;
  oss << "handle_truth foreign=" << r.foreign_vm_read
      << " syscall=" << r.syscall_path << " winapi=" << r.winapi_path
      << " hidden=" << r.hidden;
  r.detail = oss.str();
  return r;
}

}  // namespace t1_blue
