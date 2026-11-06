#pragma once

// Lab RED example for strategy `hwid_spoof` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <cstdint>
#include <string>

namespace examples::hwid_spoof {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
};

/// Multi-step red entry for `hwid_spoof`.
RedResult apply(sim::World& w);

}  // namespace examples::hwid_spoof
