#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::decoy_render_ml_evasion {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool ml_confusion_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::decoy_render_ml_evasion
