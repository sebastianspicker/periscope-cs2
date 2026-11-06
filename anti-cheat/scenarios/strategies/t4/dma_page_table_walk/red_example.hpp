#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::dma_page_table_walk {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; std::uint64_t base_pfn = 0; int entries = 0; int levels = 0; };
RedResult apply(sim::World& w);
}  // namespace examples::dma_page_table_walk
