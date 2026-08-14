#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::lag_switch {
struct BlueResult { bool detected; int signals; std::vector<std::string> reasons; double risk;
  bool mitigated = false; std::string detail; };
BlueResult detect(sim::World& w);
}  // namespace examples::lag_switch
