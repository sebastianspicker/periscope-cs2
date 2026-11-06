#pragma once

// Lab RED example for strategy `veh_exception_cf` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <cstdint>
#include <string>

namespace examples::veh_exception_cf {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
};

/// Multi-step red entry for `veh_exception_cf`.
RedResult apply(sim::World& w);

}  // namespace examples::veh_exception_cf
