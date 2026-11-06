#pragma once
// Lab BLUE for `web_phone_radar` (theme multi-reason, Simulated).
#include "sim/world.hpp"
#include <string>
#include <vector>
namespace examples::web_phone_radar {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::web_phone_radar
