#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::pattern_scan_offsets {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool bulk_read_detected = false; bool pattern_marker_found = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::pattern_scan_offsets
