#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::peb_spoof_hide {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool peb_spoof_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::peb_spoof_hide
