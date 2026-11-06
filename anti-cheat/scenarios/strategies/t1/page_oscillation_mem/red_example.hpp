#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::page_oscillation_mem {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; int cycles = 0; bool rw_flipped = false; bool rx_flipped = false; };
RedResult apply(sim::World& w);
}  // namespace examples::page_oscillation_mem
