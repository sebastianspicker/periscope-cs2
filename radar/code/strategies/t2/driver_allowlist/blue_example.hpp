#pragma once

// Lab BLUE example for strategy `driver_allowlist` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"
#include "sim/narrative.hpp"

#include <string>
#include <vector>

namespace examples::driver_allowlist {

// Lab result/type `BlueResult` used by this educational unit.
struct BlueResult {
  bool detected = false;
  int signals = 0;
  std::vector<std::string> reasons;
  double risk = 0.0;
  bool mitigated = false;
  std::string detail;
};

BlueResult detect(sim::World& w);
bool mitigate(sim::World& w);
BlueResult run_blue(sim::World& w, sim::Narrator& n);

}  // namespace examples::driver_allowlist
