#pragma once

// Lab BLUE for `delayed_ban` on sim::World only (theme multi-reason detect/mitigate).

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::delayed_ban {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
  int ticks_seen = 0;
  bool multi_tick = false;
};

BlueResult detect(sim::World& w);

}  // namespace examples::delayed_ban
