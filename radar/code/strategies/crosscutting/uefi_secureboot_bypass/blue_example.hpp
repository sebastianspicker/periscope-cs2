#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::uefi_secureboot_bypass {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  bool secure_boot_status_changed = false;
  bool unsigned_driver_detected = false;
  bool uefi_variable_modified = false;
  int detection_count = 0;  // alias of signals for pair.cpp compatibility
  std::string detail;
};

class Blue {
public:
  BlueResult detect(const sim::World& w) noexcept;
  BlueResult mitigate(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Detect Secure Boot bypass via UEFI variable monitoring, "
      "TPM PCR measurement verification, and unsigned driver detection.";
};

}  // namespace examples::uefi_secureboot_bypass
