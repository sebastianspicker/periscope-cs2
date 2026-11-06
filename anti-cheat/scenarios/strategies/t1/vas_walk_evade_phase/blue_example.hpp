#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::vas_walk_evade_phase {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool vas_evasion_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::vas_walk_evade_phase
