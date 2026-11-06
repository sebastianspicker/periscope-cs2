#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::runtime_health_ladder {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool health_ladder_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::runtime_health_ladder
