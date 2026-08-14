#pragma once

// Lab RED example for strategy `anti_re_canary` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <cstdint>
#include <string>

namespace examples::anti_re_canary {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
};

/// Multi-step red entry for `anti_re_canary`.
RedResult apply(sim::World& w);

}  // namespace examples::anti_re_canary
