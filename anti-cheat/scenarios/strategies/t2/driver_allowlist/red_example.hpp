#pragma once

// Lab RED example for strategy `driver_allowlist` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"
#include "sim/narrative.hpp"

#include <string>

namespace examples::driver_allowlist {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

RedResult apply(sim::World& w);
RedResult run_red(sim::World& w, sim::Narrator& n);

}  // namespace examples::driver_allowlist
