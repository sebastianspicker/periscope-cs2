#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::dynamic_api_table {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool dynamic_imports_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::dynamic_api_table
