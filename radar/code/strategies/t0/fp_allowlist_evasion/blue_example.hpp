#pragma once

// Blue simulation for fp_allowlist_evasion. It checks independent handle/read telemetry and the
// strategy-specific scar (reputable-looking reader co-residence) before reporting a risk-scored detection.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::fp_allowlist_evasion {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
  bool fp_only = false;
  int reason_count = 0;
};
BlueResult detect(sim::World& w);
}  // namespace examples::fp_allowlist_evasion
