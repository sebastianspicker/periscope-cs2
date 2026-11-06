#pragma once

// Red simulation for dirty_bitmap_invalidation. Red's read bursts force
// global (broadcast) invalidation storms in the blue-controlled validator,
// and red observes the storms to map blue's scan cadence. This educational
// example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::dirty_bitmap_invalidation {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::dirty_bitmap_invalidation
