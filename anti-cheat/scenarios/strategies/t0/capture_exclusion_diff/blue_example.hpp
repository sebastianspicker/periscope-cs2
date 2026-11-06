#pragma once

// Blue simulation for capture_exclusion_diff (168). Blue captures the screen,
// diffs it against the presented frame, and treats an empty diff where an
// overlay is expected as capture-exclusion evidence.
#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::capture_exclusion_diff {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::capture_exclusion_diff
