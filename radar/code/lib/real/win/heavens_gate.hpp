// heavens_gate.hpp — Heaven's Gate (WOW64 32→64 transition) helpers.
//
// Classic Heaven's Gate: a 32-bit process far-jumps to 64-bit CS to execute
// x64 syscalls while bypassing 32-bit ntdll wrappers.
//
// On native x64 builds the transition is unnecessary — indirect syscalls
// already bypass usermode hooks. This module still provides:
//   - WOW64 process detection
//   - Gate availability probe
//   - On x86-WOW64: real far-jump gate invocation of a 64-bit stub
//   - On x64: falls through to indirect syscall path (documented)

#pragma once

#include "real/platform.hpp"

#include <cstdint>

namespace real::win {

struct HeavensGateReport {
  bool wow64_process = false;
  bool gate_available = false;
  bool used_gate = false;
  bool used_native_syscall = false;
  std::uint64_t result = 0;
  const char* detail = "";
};

/// True if the current process is running under WOW64 (32-bit on 64-bit OS).
bool is_wow64_process() noexcept;

/// True if Heaven's Gate technique can be used in this process.
bool heavens_gate_available() noexcept;

/// Execute NtClose-style no-op via Heaven's Gate when available, else via
/// native indirect syscall. Used to exercise the real code path in tests.
HeavensGateReport heavens_gate_probe() noexcept;

}  // namespace real::win
