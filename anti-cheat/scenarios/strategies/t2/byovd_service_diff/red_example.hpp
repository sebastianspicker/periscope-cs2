#pragma once

// Red simulation for byovd_service_diff (167). Red loads a signed vulnerable
// driver, uses it, then deletes the SCM service and registry key. The module
// hash remains in PsLoadedModuleList, which is the artifact cleanup misses.
#include "sim/world.hpp"

#include <string>

namespace examples::byovd_service_diff {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::byovd_service_diff
