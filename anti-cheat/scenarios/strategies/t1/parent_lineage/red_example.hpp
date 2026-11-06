#pragma once

// Lab RED example for strategy `parent_lineage` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::parent_lineage {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `parent_lineage`.
RedResult apply(sim::World& w);

}  // namespace examples::parent_lineage
