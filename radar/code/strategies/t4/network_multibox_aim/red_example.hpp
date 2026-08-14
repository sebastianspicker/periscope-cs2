#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::network_multibox_aim {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::network_multibox_aim
