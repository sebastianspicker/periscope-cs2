#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::hud_radar_parsing {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int entities_found = 0; int nodes_walked = 0; };
RedResult apply(sim::World& w);
}  // namespace examples::hud_radar_parsing
