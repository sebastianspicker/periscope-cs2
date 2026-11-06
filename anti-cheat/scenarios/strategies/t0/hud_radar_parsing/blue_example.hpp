#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::hud_radar_parsing {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool radar_snapshot_seen = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::hud_radar_parsing
