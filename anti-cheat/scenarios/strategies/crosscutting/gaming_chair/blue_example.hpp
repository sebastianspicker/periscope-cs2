#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::gaming_chair {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  std::vector<std::string> detected_features;
  int count = 0;
  double risk = 0.0;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::gaming_chair
