// module_hide.hpp — Unlink the current (or target) module from PEB LDR lists.
//
// Removes the LDR_DATA_TABLE_ENTRY from:
//   - InLoadOrderModuleList
//   - InMemoryOrderModuleList
//   - InInitializationOrderModuleList
//
// Module remains mapped; VAC/AC that walk PEB module lists miss it.
// VAS walk / VAD still sees the region — pair with pe_hide for headers.

#pragma once

#include "real/platform.hpp"

#include <cstdint>

namespace real::win {

struct ModuleHideReport {
  bool unlinked = false;
  bool relinked = false;
  std::uint64_t module_base = 0;
  const char* detail = "";
};

/// Unlink module by base. Pass 0 for the main image (PEB ImageBaseAddress).
ModuleHideReport module_unlink(std::uint64_t module_base = 0) noexcept;

/// Relink a previously unlinked module (restores list pointers).
ModuleHideReport module_relink() noexcept;

/// True if a module is currently unlinked by this module.
bool module_is_hidden() noexcept;

}  // namespace real::win
