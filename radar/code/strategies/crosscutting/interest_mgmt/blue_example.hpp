#pragma once

// Lab BLUE for `interest_mgmt` on sim::World only (theme multi-reason detect/mitigate).

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::interest_mgmt {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  std::string detail;
  int enemies_culled = 0;
  int replicated = 0;
};

BlueResult detect(sim::World& w);

}  // namespace examples::interest_mgmt
