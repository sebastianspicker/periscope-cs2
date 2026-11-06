#pragma once

// Lab RED example for strategy `offset_c2` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <cstdint>
#include <string>

namespace examples::offset_c2 {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
};

/// Multi-step red entry for `offset_c2`.
RedResult apply(sim::World& w);

}  // namespace examples::offset_c2
