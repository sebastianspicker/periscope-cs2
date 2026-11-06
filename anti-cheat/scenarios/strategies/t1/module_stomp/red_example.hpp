#pragma once

// Lab RED example for strategy `module_stomp` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::module_stomp {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `module_stomp`.
RedResult apply(sim::World& w);

}  // namespace examples::module_stomp
