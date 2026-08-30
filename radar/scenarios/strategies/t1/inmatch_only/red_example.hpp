#pragma once

// Lab RED example for strategy `inmatch_only` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::inmatch_only {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `inmatch_only`.
RedResult apply(sim::World& w);

}  // namespace examples::inmatch_only
