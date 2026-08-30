// etw_blind.hpp — Blind usermode ETW providers by patching ntdll writers.
//
// Targets (resolved via PEB+EAT, never IAT):
//   - EtwEventWrite
//   - EtwEventWriteFull
//   - NtTraceEvent (when present)
//
// Patch: write `xor eax,eax; ret` (33 C0 C3) over the prologue so consumers
// see STATUS_SUCCESS with no event. Original bytes saved for restore.
//
// SCAR: .text page becomes dirty vs disk image; blue can detect via
// section hash / known-good comparison. Kernel ETW TI may still fire.

#pragma once

#include "real/platform.hpp"

#include <cstddef>
#include <cstdint>

namespace real::win {

struct EtwBlindReport {
  bool patched = false;
  int targets_patched = 0;
  int targets_found = 0;
  const char* detail = "";
};

/// Patch ETW write stubs. Idempotent. Returns how many were patched.
EtwBlindReport etw_blind_apply() noexcept;

/// Restore original prologues. Safe if never applied.
EtwBlindReport etw_blind_restore() noexcept;

/// True if a previous apply is still active.
bool etw_blind_active() noexcept;

}  // namespace real::win
