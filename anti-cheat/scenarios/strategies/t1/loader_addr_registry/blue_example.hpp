#pragma once

// Blue simulation for loader_addr_registry. It scores the data-section patch
// scar, the decode-time access burst, and the encrypted-registry artifact
// with independent reasons before reporting a risk-scored verdict.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::loader_addr_registry {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::loader_addr_registry
