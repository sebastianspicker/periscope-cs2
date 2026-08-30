#pragma once

// Naive blue: only watches usermode API hooks (ntdll OpenProcess/RPM stubs).
// Correct blue for T1: handle truth — path (syscall vs winapi) is irrelevant.

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t1_blue {

// Aggregate outcome fields for `HookTrapReport` (lab narrative / tests).
struct HookTrapReport {
  int visible_winapi_opens = 0;
  int syscall_opens_missed = 0;
  bool blind_to_syscall_red = false;
  std::string detail;
};

// Lab type `UsermodeHookTrap` used by this educational unit.
class UsermodeHookTrap {
 public:
  /// True if a non-syscall-path VM_READ open would cross hooked stubs.
  bool would_see_attach(const sim::Handle& h) const;

  int count_visible_opens(const sim::World& w, std::uint32_t game_pid) const;

  /// Full report: winapi visible vs syscall missed.
  HookTrapReport analyze(const sim::World& w, std::uint32_t game_pid) const;
};

// Aggregate outcome fields for `HandleTruthReport` (lab narrative / tests).
struct HandleTruthReport {
  int foreign_vm_read = 0;
  int syscall_path = 0;
  int winapi_path = 0;
  int hidden = 0;
  std::vector<std::string> owners;
  std::string detail;
};

// Lab type `HandleTruthMonitor` used by this educational unit.
class HandleTruthMonitor {
 public:
  int count_vm_read_handles(const sim::World& w, std::uint32_t game_pid) const;

  /// Multi-step truth: count all foreign VM_READ including hidden/syscall.
  HandleTruthReport analyze(const sim::World& w, std::uint32_t game_pid,
                            bool include_hidden = true) const;
};

}  // namespace t1_blue
