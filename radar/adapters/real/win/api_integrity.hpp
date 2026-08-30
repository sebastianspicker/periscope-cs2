// api_integrity.hpp — Real-path API pointer and prologue integrity checks.
//
// Verifies that resolved function pointers land inside the expected module
// image and that prologues are not inline-hooked (jmp/call/push-ret).

#pragma once

#include "real/platform.hpp"

#include <cstddef>
#include <cstdint>

namespace real::win {

enum class HookKind : std::uint8_t {
  None = 0,
  InlineJmp,
  InlineCall,
  PushRet,
  Int3,
  OutsideModule,
  Unknown,
};

struct IntegrityFinding {
  const char* module = "";
  const char* function = "";
  std::uint64_t address = 0;
  HookKind hook = HookKind::None;
  bool ok = true;
};

struct IntegrityReport {
  int checked = 0;
  int failed = 0;
  int hooked = 0;
  IntegrityFinding findings[32]{};
  int finding_count = 0;
};

/// Verify a single resolved pointer against its module + prologue.
IntegrityFinding verify_pointer(const char* module, const char* function,
                                void* resolved) noexcept;

/// Verify the critical subset of g_Api() pointers.
IntegrityReport verify_api_table() noexcept;

/// Classify prologue bytes at `code`.
HookKind classify_prologue(const std::uint8_t* code, std::size_t len = 16) noexcept;

}  // namespace real::win
