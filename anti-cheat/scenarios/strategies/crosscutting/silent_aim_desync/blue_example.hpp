#pragma once
// Lab BLUE for `silent_aim_desync` (theme multi-reason, Simulated).
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::silent_aim_desync {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::silent_aim_desync
