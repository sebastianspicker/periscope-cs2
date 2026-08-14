#pragma once

// Lab RED example for strategy `speedhack_timescale` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::speedhack_timescale {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `speedhack_timescale`.
RedResult apply(sim::World& w);

}  // namespace examples::speedhack_timescale
