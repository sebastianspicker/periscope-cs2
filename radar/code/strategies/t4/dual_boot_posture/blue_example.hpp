#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::dual_boot_posture {
struct BlueResult { bool detected; int signals; std::vector<std::string> reasons; double risk;
  bool mitigated = false; std::string detail; };
BlueResult detect(sim::World& w);
}  // namespace examples::dual_boot_posture
