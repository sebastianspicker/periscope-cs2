#pragma once

// Lab BLUE example for strategy `hwid_spoof` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>

namespace examples::hwid_spoof {

// Lab result/type `BlueResult` used by this educational unit.
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};

/// Multi-reason blue entry for `hwid_spoof`.
BlueResult detect(sim::World& w);

}  // namespace examples::hwid_spoof
