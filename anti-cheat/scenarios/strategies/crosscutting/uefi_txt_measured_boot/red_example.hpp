#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::uefi_txt_measured_boot {

struct RedResult {
  bool achieved = false;
  bool event_log_tampered = false;
  bool pcr_value_spoofed = false;
  int events_removed = 0;
  int steps = 0;
  std::string detail;
};

class Red {
public:
  void apply(sim::World& w) noexcept;
  RedResult apply_detailed(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Modify the TPM event log after a measured launch to hide an "
      "early-launch component. Requires SMM or firmware-level access "
      "to the TPM2 configuration table in UEFI firmware.";
};

}  // namespace examples::uefi_txt_measured_boot
