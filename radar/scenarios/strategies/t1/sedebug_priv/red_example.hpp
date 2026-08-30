#pragma once

// Lab RED example for strategy `sedebug_priv` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::sedebug_priv {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `sedebug_priv`.
RedResult apply(sim::World& w);

}  // namespace examples::sedebug_priv
