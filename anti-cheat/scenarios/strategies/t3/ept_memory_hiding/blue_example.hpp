#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::ept_memory_hiding {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool ept_hiding_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::ept_memory_hiding
