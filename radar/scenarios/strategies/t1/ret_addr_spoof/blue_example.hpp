#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace strategy::t1_ret_addr_spoof {

struct BlueResult {
  int chain_frames_checked{0};
  int suspicious_frames{0};
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
      "Walk thread return address chain, flag non-module and RWX addresses";
};

}  // namespace strategy::t1_ret_addr_spoof
