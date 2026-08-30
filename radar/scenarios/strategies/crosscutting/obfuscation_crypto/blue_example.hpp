#pragma once

// Lab BLUE example for strategy `obfuscation_crypto` on sim::World only.
// Multi-reason educational detect/mitigate on World scars.

#include "sim/world.hpp"

#include <string>

namespace examples::obfuscation_crypto {

// Lab result/type `BlueResult` used by this educational unit.
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  std::string detail;
};

/// Multi-reason blue entry for `obfuscation_crypto`.
BlueResult detect(sim::World& w);

}  // namespace examples::obfuscation_crypto
