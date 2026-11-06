#pragma once

// Lab RED example for strategy `polymorphic_build` on sim::World only.
// Multi-step educational scars: per-build layout + watermark + module hash.

#include "sim/world.hpp"

#include <cstdint>
#include <string>

namespace examples::polymorphic_build {

struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  int steps = 0;
  std::string detail;
};

/// Multi-step red entry for `polymorphic_build`.
RedResult apply(sim::World& w);

}  // namespace examples::polymorphic_build
