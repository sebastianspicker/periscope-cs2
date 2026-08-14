#pragma once

// Red simulation for internal_footprint. It plants the observable scar: manual-map footprint suppression claims.
// This educational example mutates only sim::World and never performs OS actions.
#include "sim/world.hpp"
#include <string>

namespace examples::internal_footprint {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::internal_footprint
