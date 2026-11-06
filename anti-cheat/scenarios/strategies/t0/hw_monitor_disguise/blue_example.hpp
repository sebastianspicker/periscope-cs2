#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::hw_monitor_disguise {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool disguise_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::hw_monitor_disguise
