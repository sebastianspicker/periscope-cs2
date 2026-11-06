#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::dma_hardware {
struct BlueResult { bool detected; int signals; std::vector<std::string> reasons; double risk;
  bool mitigated = false; std::string detail; };
BlueResult detect(sim::World& w);
}  // namespace examples::dma_hardware
