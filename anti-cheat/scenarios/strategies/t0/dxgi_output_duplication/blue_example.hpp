#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::dxgi_output_duplication {
struct BlueResult { bool detected = false; bool mitigated = false; int signals = 0; std::vector<std::string> reasons; std::string detail; bool output_dup_detected = false; };
BlueResult detect(sim::World& w);
}  // namespace examples::dxgi_output_duplication
