#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::vas_walk_evade_phase {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int evasion_count = 0; bool page_hidden = false; bool region_reshuffled = false; };
RedResult apply(sim::World& w);
}  // namespace examples::vas_walk_evade_phase
