#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::page_oscillation_mem {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool oscillation_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::page_oscillation_mem
