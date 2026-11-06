#pragma once

// Lab BLUE example for strategy `veh_exception_cf` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>

namespace examples::veh_exception_cf {

// Lab result/type `BlueResult` used by this educational unit.
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};

/// Multi-reason blue entry for `veh_exception_cf`.
BlueResult detect(sim::World& w);

}  // namespace examples::veh_exception_cf
