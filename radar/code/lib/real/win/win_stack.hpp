// win_stack.hpp — Single entry point for the T1 Windows usermode stack.
//
// Initializes and composes:
//   ApiTable PEB+EAT resolution, SSN table, hook detection, anti-debug,
//   optional ntdll restore, VEH, stack-spoof gadgets.
//
// Call win_stack_init() once at process start (radar / cheat client).

#pragma once

#include "real/platform.hpp"

#include <cstdint>

namespace real::win {

struct WinStackReport {
  bool api_resolved = false;
  bool syscalls_resolved = false;
  bool hooks_initialized = false;
  bool ntdll_restored = false;
  bool anti_debug_cleared = false;
  bool veh_registered = false;
  bool stack_spoof_ready = false;
  int ssn_nt_read = -1;
  int ssn_nt_open = -1;
  int ssn_nt_close = -1;
  int api_integrity_failed = 0;
  int ntdll_hooks_before = 0;
  int ntdll_hooks_after = 0;
  const char* detail = "";
};

struct WinStackOptions {
  bool restore_ntdll = true;
  bool clear_debug = true;
  bool register_veh = true;
  bool init_hook_detect = true;
  bool apply_etw_blind = false;  // off by default — visible .text scar
  bool hide_module = false;     // off by default — hard to reverse if misused
};

/// Initialize the full Windows T1 stack. Idempotent.
WinStackReport win_stack_init(const WinStackOptions& opts = {}) noexcept;

/// Tear down optional patches (ETW blind, VEH, PE header, module relink).
void win_stack_shutdown() noexcept;

/// True after a successful win_stack_init().
bool win_stack_ready() noexcept;

/// Last init report (valid after win_stack_init).
const WinStackReport& win_stack_last_report() noexcept;

}  // namespace real::win
