#pragma once

// Lab RED example for strategy `staged_loader` on sim::World only.
// Multi-step educational scars: stage-one → C2 → encrypted stage-two → map.

#include "sim/world.hpp"

#include <cstdint>
#include <string>

namespace examples::staged_loader {

struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `staged_loader`.
RedResult apply(sim::World& w);

}  // namespace examples::staged_loader
