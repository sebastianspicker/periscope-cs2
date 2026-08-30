#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::windowless_swapchain {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}
