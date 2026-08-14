#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::dma_page_table_walk {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool dma_walk_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::dma_page_table_walk
