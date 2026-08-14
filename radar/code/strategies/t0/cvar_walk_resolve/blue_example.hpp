#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::cvar_walk_resolve {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool resolve_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::cvar_walk_resolve
