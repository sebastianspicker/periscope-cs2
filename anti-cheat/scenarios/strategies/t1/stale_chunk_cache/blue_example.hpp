#pragma once

// Blue simulation for stale_chunk_cache. Blue deliberately reallocates the
// container (churn pressure), tags chunks with generation counters, and
// detects red's stale reads as generation mismatches against the memoized
// pointers red left behind.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::stale_chunk_cache {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::stale_chunk_cache
