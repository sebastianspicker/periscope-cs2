#pragma once

// Blue simulation for torn_snapshot_artifacts. It validates the records red
// assembled against plausibility invariants (health/team/lifestate/origin
// combinations) and scores the multi-transaction read pattern that produced
// them.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::torn_snapshot_artifacts {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::torn_snapshot_artifacts
