#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::runtime_health_ladder {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int level = 0; int transitions = 0; };
RedResult apply(sim::World& w);
}  // namespace examples::runtime_health_ladder
