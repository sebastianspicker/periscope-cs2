#pragma once

// Lab BLUE for `fallback_chain` on sim::World only (theme multi-reason detect/mitigate).

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::fallback_chain {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
  bool handle_hit = false;
  bool driver_hit = false;
};

BlueResult detect(sim::World& w);

}  // namespace examples::fallback_chain
