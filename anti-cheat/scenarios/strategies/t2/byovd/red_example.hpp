#pragma once

// Lab RED example for strategy `byovd` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"
#include "sim/narrative.hpp"

#include <string>

namespace examples::byovd {

struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

RedResult apply(sim::World& w);
// Legacy narrated entry retained for catalog and API tests.
RedResult run_red(sim::World& w, sim::Narrator& n);

}  // namespace examples::byovd
