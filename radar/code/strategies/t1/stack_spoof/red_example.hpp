#pragma once

// Lab RED example for strategy `stack_spoof` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::stack_spoof {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `stack_spoof`.
RedResult apply(sim::World& w);

}  // namespace examples::stack_spoof
