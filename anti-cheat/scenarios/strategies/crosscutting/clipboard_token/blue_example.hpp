#pragma once

// Lab BLUE example for strategy `clipboard_token` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>

namespace examples::clipboard_token {

// Lab result/type `BlueResult` used by this educational unit.
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};

/// Multi-reason blue entry for `clipboard_token`.
BlueResult detect(sim::World& w);

}  // namespace examples::clipboard_token
