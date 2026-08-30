#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::temporal_phase_evasion {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool temporal_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::temporal_phase_evasion
