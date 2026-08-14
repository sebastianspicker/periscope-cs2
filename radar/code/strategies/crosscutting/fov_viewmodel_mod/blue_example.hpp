#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::fov_viewmodel_mod {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::fov_viewmodel_mod
