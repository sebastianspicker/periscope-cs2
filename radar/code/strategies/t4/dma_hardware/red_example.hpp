#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::dma_hardware {
struct RedResult { bool achieved; int steps; std::string detail; };
RedResult apply(sim::World& w);
}  // namespace examples::dma_hardware
