#pragma once
#include "sim/world.hpp"
#include <string>
namespace examples::multimap_radar_share {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::multimap_radar_share
