#pragma once

// Lab RED example for strategy `manual_map_hide` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::manual_map_hide {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `manual_map_hide`.
RedResult apply(sim::World& w);

}  // namespace examples::manual_map_hide
