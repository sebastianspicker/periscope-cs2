#pragma once

// Blue simulation for byovd_service_diff (167). Blue hashes loaded modules and
// diffs the service database; a loaded hash with no service entry is a
// cleaned-but-still-resident vulnerable driver.
#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::byovd_service_diff {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::byovd_service_diff
