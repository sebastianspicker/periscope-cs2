#pragma once

// Lab BLUE example for strategy `anti_re_canary` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>

namespace examples::anti_re_canary {

// Lab result/type `BlueResult` used by this educational unit.
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};

/// Multi-reason blue entry for `anti_re_canary`.
BlueResult detect(sim::World& w);

}  // namespace examples::anti_re_canary
