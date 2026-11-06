#pragma once

// Red simulation for torn_snapshot_artifacts. It reads entity fields in
// several separate transactions while the game state advances between them,
// producing torn/garbled records, and keeps a single-transaction control read
// for contrast. This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::torn_snapshot_artifacts {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::torn_snapshot_artifacts
