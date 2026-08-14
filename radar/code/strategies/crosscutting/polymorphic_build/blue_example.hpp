#pragma once

// Lab BLUE example for strategy `polymorphic_build` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace examples::polymorphic_build {

struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  std::vector<std::string> reasons;
  std::string detail;
};

/// Multi-reason blue entry for `polymorphic_build`.
BlueResult detect(sim::World& w);

}  // namespace examples::polymorphic_build
