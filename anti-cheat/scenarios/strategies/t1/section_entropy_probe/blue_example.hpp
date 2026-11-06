#pragma once

// Blue simulation for section_entropy_probe (166). Blue measures non-image
// section entropy and samples the decode window; register-only decoding has
// no data-section patch to catch, so entropy becomes the primary scar.
#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::section_entropy_probe {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::section_entropy_probe
