#pragma once

// Blue simulation for auditor_introspection_spoof. It verifies the auditor's
// CRC report against itself (CRC-of-CRC), re-reads prologue bytes through a
// second path, and cross-checks diagnostic dumps, with independent reasons.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::auditor_introspection_spoof {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::auditor_introspection_spoof
