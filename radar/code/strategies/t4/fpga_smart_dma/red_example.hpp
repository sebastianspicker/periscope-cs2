#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::fpga_smart_dma {
struct RedResult { bool achieved; int steps; int scatter_reads; bool hidden_rescan; std::string detail; };
RedResult apply(sim::World& w);
}
