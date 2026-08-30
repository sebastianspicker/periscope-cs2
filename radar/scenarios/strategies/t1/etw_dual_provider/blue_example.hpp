#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace strategy::t1_etw_dual_provider {

struct BlueResult {
  bool ti_down{false};
  bool secondary_detected{false};
  bool detected{false};
  bool mitigated{false};
  int signals{0};
  std::vector<std::string> reasons;
  std::string detail;
};

class Blue {
 public:
  BlueResult detect(const sim::World& w) noexcept;
  BlueResult mitigate(sim::World& w) noexcept;
  static constexpr const char* kDescription =
      "Maintain redundant ETW providers — secondary catches events when TI is blinded";
};

}  // namespace strategy::t1_etw_dual_provider
