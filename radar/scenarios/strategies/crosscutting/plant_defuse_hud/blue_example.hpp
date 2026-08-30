#pragma once
// Lab BLUE: multi-reason HUD timing alert residual (≠ bomb fuse product alone).
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::plant_defuse_hud {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  bool plant_alert = false;
  bool defuse_alert = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::plant_defuse_hud
