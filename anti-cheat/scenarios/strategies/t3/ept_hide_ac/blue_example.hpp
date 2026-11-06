#pragma once
#include "sim/narrative.hpp"
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::ept_hide_ac {
struct BlueResult { bool detected; int signals; std::vector<std::string> reasons; double risk;
  bool mitigated = false; std::string detail; };
BlueResult detect(sim::World& w);
BlueResult run_blue(sim::World& w, sim::Narrator& n);
}  // namespace examples::ept_hide_ac
