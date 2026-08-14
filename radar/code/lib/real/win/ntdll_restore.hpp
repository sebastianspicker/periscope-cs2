// ntdll_restore.hpp — Restore a clean ntdll .text from KnownDlls / disk.
//
// Technique: map a pristine ntdll image via \KnownDlls\ntdll.dll (section
// object, no disk I/O scar of CreateFile on System32) and copy the clean
// .text over the in-memory image after making pages writable.
//
// This defeats usermode inline hooks on NT stubs (EDR/AC user hooks).
// Kernel-level hooks and ETW TI remain effective.

#pragma once

#include "real/platform.hpp"

#include <cstddef>
#include <cstdint>

namespace real::win {

struct NtdllRestoreReport {
  bool mapped_clean = false;
  bool text_restored = false;
  std::size_t bytes_patched = 0;
  std::size_t hooks_cleared = 0;
  std::uint64_t clean_base = 0;
  std::uint64_t loaded_base = 0;
  const char* detail = "";
};

/// Map clean ntdll, restore .text of the loaded image, unmap clean view.
/// Safe to call multiple times; second call is a no-op if already clean.
NtdllRestoreReport restore_ntdll_text() noexcept;

/// True if loaded ntdll .text currently matches a fresh KnownDlls mapping.
bool ntdll_text_is_clean() noexcept;

/// Count inline-hook prologues among a fixed set of critical NT exports.
std::size_t count_ntdll_hooks() noexcept;

}  // namespace real::win
