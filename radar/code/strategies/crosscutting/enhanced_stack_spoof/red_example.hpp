#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::enhanced_stack_spoof {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}
