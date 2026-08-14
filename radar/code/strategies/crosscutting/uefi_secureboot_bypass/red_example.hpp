#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::uefi_secureboot_bypass {

struct RedResult {
  bool achieved = false;
  bool secure_boot_disabled = false;
  bool custom_db_installed = false;
  bool unsigned_driver_allowed = false;
  int steps = 0;
  std::string detail;
};

class Red {
public:
  RedResult apply(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Bypass Secure Boot by installing a custom db (allowed signature) "
      "entry or disabling Secure Boot via UEFI runtime SetVariable. "
      "Allows loading unsigned kernel drivers for T2/T3 operations.";
};

}  // namespace examples::uefi_secureboot_bypass
