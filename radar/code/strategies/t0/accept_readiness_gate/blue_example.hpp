#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::accept_readiness_gate {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool readiness_bypassed = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::accept_readiness_gate
