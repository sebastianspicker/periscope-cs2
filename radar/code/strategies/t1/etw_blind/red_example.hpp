#pragma once

// Lab RED example for strategy `etw_blind` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::etw_blind {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `etw_blind`.
RedResult apply(sim::World& w);

}  // namespace examples::etw_blind
