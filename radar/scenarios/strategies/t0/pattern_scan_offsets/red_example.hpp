#pragma once
#include "sim/world.hpp"
#include <string>

namespace examples::pattern_scan_offsets {
struct RedResult { bool achieved = false; int steps = 0; std::string detail; bool rip_resolved = false; int patterns_scanned = 0; };
RedResult apply(sim::World& w);
}  // namespace examples::pattern_scan_offsets
