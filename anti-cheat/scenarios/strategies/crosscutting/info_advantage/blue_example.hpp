#pragma once

// Lab BLUE for `info_advantage` on sim::World only (theme multi-reason detect/mitigate).

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::info_advantage {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  std::string detail;
  double score = 0;
  bool multi_reason = false;
};

BlueResult detect(sim::World& w);

}  // namespace examples::info_advantage
