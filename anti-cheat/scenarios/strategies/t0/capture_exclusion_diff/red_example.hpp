#pragma once

// Red simulation for capture_exclusion_diff (168). Red marks the overlay
// window WDA_EXCLUDEFROMCAPTURE so a capture-based sensor sees the clean game
// frame while the player sees outlines. Sim only.
#include "sim/world.hpp"

#include <string>

namespace examples::capture_exclusion_diff {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::capture_exclusion_diff
