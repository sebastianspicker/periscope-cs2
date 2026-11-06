#pragma once

// Blue simulation for dirty_bitmap_invalidation. Blue detects that its own
// broadcast invalidation storms leak scan-cadence intel to a foreign observer
// and mitigates by adopting dirty-bitmap selective invalidation: only flagged
// consumers flush.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::dirty_bitmap_invalidation {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::dirty_bitmap_invalidation
