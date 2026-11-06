#pragma once

// Blue simulation for escalation_cost_curve. Blue observes the persistent
// mutation footprints each access rung left behind (testsigning, HVCI,
// blocklist/registry toggles, pending reboot, driver, handle scars) and
// prices the escalation into a risk-scored verdict.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::escalation_cost_curve {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::escalation_cost_curve
