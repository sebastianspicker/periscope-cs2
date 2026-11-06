#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::cvar_walk_resolve {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int tier_reached = 0; int entries_found = 0; };
RedResult apply(sim::World& w);
}  // namespace examples::cvar_walk_resolve
