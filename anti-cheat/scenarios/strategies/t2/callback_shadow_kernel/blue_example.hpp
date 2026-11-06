#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::callback_shadow_kernel {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool callback_anomaly = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::callback_shadow_kernel
