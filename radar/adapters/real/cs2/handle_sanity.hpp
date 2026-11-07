// handle_sanity.hpp: Source-2 handle sentinel validation (E63 / S4).
//
// A torn or stale read can resolve `0xffffffff`, `0xffffffffffffffff`, or a
// tiny value that is not a mapped address. Resolving those produces
// plausible-looking garbage (team 0, health 0, origin at the origin) instead
// of a clean miss. The checks are deliberately cheap and pure.
//
// Pure and host-testable.

#pragma once

#include <cstdint>

namespace real::cs2::handle {

inline constexpr std::uint32_t kInvalidHandle = 0xFFFFFFFFu;
inline constexpr std::uint64_t kInvalidPointer = 0xFFFFFFFFFFFFFFFFull;
inline constexpr std::uint64_t kMinUserPointer = 0x10000ull;
inline constexpr std::uint32_t kMaxEntityIndex = 0x3FFFu;

/// A 32-bit entity handle that can index the identity list.
inline bool handle_usable(std::uint32_t handle) {
  if (handle == 0 || handle == kInvalidHandle) return false;
  const std::uint32_t index = handle & 0x7FFFu;
  return index != 0 && index <= kMaxEntityIndex;
}

/// A 64-bit object pointer that can plausibly be dereferenced.
inline bool pointer_usable(std::uint64_t pointer) {
  return pointer != 0 && pointer != kInvalidPointer &&
         pointer >= kMinUserPointer;
}

}  // namespace real::cs2::handle
