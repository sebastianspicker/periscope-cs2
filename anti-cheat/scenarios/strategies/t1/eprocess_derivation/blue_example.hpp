#pragma once

// Blue simulation for eprocess_derivation. It scores the derivation-probe
// read-scar pattern and the version-churn pressure runtime derivation puts on
// the sensor fleet, with independent reasons and a continuous re-validation
// mitigation.
#include "sim/world.hpp"
#include <string>
#include <vector>

namespace examples::eprocess_derivation {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int signals = 0;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::eprocess_derivation
