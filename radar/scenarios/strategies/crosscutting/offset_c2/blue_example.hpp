#pragma once

// Lab BLUE example for strategy `offset_c2` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>

namespace examples::offset_c2 {

// Lab result/type `BlueResult` used by this educational unit.
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};

/// Multi-reason blue entry for `offset_c2`.
BlueResult detect(sim::World& w);

}  // namespace examples::offset_c2
