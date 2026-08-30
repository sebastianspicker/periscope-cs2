#pragma once

// Lab RED example for strategy `raw_vs_sendinput` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::raw_vs_sendinput {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `raw_vs_sendinput`.
RedResult apply(sim::World& w);

}  // namespace examples::raw_vs_sendinput
