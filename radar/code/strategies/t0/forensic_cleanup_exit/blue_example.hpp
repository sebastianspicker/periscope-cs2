#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::forensic_cleanup_exit {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool cleanup_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::forensic_cleanup_exit
