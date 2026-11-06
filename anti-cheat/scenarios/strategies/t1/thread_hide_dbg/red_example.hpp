#pragma once

// Lab RED example for strategy `thread_hide_dbg` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::thread_hide_dbg {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `thread_hide_dbg`.
RedResult apply(sim::World& w);

}  // namespace examples::thread_hide_dbg
