#pragma once
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::composition_radar_loop {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  bool fog = false;
  bool pattern_hit = false;
  bool cs_hit = false;
  bool fog_hit = false;
  std::vector<std::string> reasons;
};
BlueResult detect(sim::World& w);
}  // namespace examples::composition_radar_loop
