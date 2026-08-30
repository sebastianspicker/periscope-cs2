#pragma once

// Lab RED example for strategy `process_hollow` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::process_hollow {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `process_hollow`.
RedResult apply(sim::World& w);

}  // namespace examples::process_hollow
