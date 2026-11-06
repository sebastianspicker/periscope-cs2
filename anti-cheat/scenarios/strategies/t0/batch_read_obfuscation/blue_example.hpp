#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::batch_read_obfuscation {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool scatter_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::batch_read_obfuscation
