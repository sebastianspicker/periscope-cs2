#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::uefi_txt_measured_boot {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  bool event_log_tampered = false;
  bool pcr_mismatch = false;
  int missing_entries = 0;
  int detection_count = 0;  // alias of signals for pair.cpp compatibility
  std::string detail;
};

class Blue {
public:
  BlueResult detect(const sim::World& w) noexcept;
  BlueResult mitigate(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Verify TPM event log integrity: recalculate PCR values from "
      "the event log and compare against signed PCR quotes. Detect "
      "event log tampering via sequence count and entry hash chains.";
};

}  // namespace examples::uefi_txt_measured_boot
