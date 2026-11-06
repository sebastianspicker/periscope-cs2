#pragma once

// Red simulation for auditor_introspection_spoof. It plants the
// auditor-introspection spoof scars: rewrite the CRC the in-game auditor's
// module scan reports, null diagnostic dumps overlapping the cheat image, and
// feed original prologue bytes back through a string-copy path.
// This educational example mutates only sim::World.
#include "sim/world.hpp"
#include <string>

namespace examples::auditor_introspection_spoof {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::auditor_introspection_spoof
