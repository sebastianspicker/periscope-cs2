#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace strategy::t1_bsecure_allowed_evade {

struct BlueResult {
  bool stomp_detected{false};
  bool signature_valid{false};
  bool timestamp_mismatch{false};
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
      "BSecureAllowed checks module signature, timestamp, size against whitelist criteria";
};

}  // namespace strategy::t1_bsecure_allowed_evade
