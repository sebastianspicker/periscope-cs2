#pragma once

// Lab RED example for strategy `indirect_syscall` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <string>

namespace examples::indirect_syscall {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `indirect_syscall`.
RedResult apply(sim::World& w);

}  // namespace examples::indirect_syscall
