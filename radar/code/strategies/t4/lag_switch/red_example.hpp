#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::lag_switch {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::lag_switch
