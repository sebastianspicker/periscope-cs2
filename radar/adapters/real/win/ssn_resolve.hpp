// ssn_resolve.hpp — Hell's Gate / Halo's Gate / Tartarus' Gate SSN recovery.
//
// When EDR/AC hooks ntdll stubs (jmp/call at prologue), the classic
// "read mov eax, SSN; syscall; ret" pattern fails. These algorithms recover
// the correct System Service Number by walking neighboring unhooked Zw*
// exports and adjusting the index delta.
//
// Order of attempts for resolve_ssn_robust():
//   1. Hell's Gate  — direct stub parse (clean ntdll)
//   2. Halo's Gate  — neighbor walk by address when target is hooked
//   3. Tartarus' Gate — scan deeper into the function body past a JMP hook
//   4. Sorted-export index — final fallback using Zw* address order

#pragma once

#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64

#include <cstddef>
#include <cstdint>

namespace real::win {

enum class SsnMethod : std::uint8_t {
  None = 0,
  HellsGate,      // clean stub at export
  HalosGate,      // recovered from neighbor
  TartarusGate,   // recovered past inline JMP
  SortedIndex,    // recovered from Zw* address sort order
};

struct SsnResult {
  int number = -1;
  SsnMethod method = SsnMethod::None;
  bool was_hooked = false;
};

/// Resolve a single NT/Zw export's SSN with full hooked-stub recovery.
SsnResult resolve_ssn_robust(const char* nt_name) noexcept;

/// Convenience: returns SSN or -1.
int resolve_ssn_number(const char* nt_name) noexcept;

/// Detect whether a function prologue looks like an inline hook.
bool is_stub_hooked(const std::uint8_t* code, std::size_t max_scan = 32) noexcept;

/// Extract SSN from a clean x64 ntdll stub at `code` (or offset within).
/// Returns -1 if the classic pattern is not found within max_scan bytes.
int extract_ssn_from_stub(const std::uint8_t* code, std::size_t max_scan = 32) noexcept;

}  // namespace real::win

#endif  // LR_PLATFORM_WINDOWS && LR_ARCH_X64
