#pragma once

// Lab RED example for strategy `mapper_artifact` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::mapper_artifact {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `mapper_artifact`.
RedResult apply(sim::World& w);

}  // namespace examples::mapper_artifact
