#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace strategy::t2_module_list_hide {

struct BlueResult {
  int expected_count{};
  int found_count{};
  bool discrepancy_detected{false};
  bool pe_header_scan_detected{false};
  bool detected{false};
  bool mitigated{false};
  int detection_count{0};
  int signals{0};
  std::vector<std::string> reasons;
  std::string detail;
};

class Blue {
 public:
  BlueResult detect(const sim::World& w) noexcept;
  BlueResult mitigate(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Capture module list via CModuleListSnapshot::Capture and "
      "cross-reference with expected known-good modules from "
      "BSecureAllowed. Detect hidden modules by scanning for "
      "PE header signatures in memory ranges without module entries.";
};

}  // namespace strategy::t2_module_list_hide
