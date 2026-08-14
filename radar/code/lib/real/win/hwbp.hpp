// hwbp.hpp — Hardware breakpoint (DR0–DR7) detection and clearance.
//
// Anti-debug / anti-analysis: AC and reverse engineers set DRx breakpoints.
// We detect non-zero DR0–DR3 or local-enable bits in DR7, and can clear them.

#pragma once

#include "real/platform.hpp"

#include <cstdint>

namespace real::win {

struct HwbpState {
  std::uint64_t dr0 = 0;
  std::uint64_t dr1 = 0;
  std::uint64_t dr2 = 0;
  std::uint64_t dr3 = 0;
  std::uint64_t dr6 = 0;
  std::uint64_t dr7 = 0;
  bool any_active = false;
};

/// Read debug registers for the current thread.
HwbpState hwbp_read() noexcept;

/// True if any hardware breakpoint is armed on this thread.
bool hwbp_detected() noexcept;

/// Clear all DRx breakpoints on the current thread.
bool hwbp_clear() noexcept;

}  // namespace real::win
