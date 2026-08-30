#pragma once

// Lab RED example for strategy `obfuscation_crypto` on sim::World only.
// Multi-step educational scars.

#include "sim/world.hpp"

#include <cstdint>
#include <string>

namespace examples::obfuscation_crypto {

// Lab result/type `RedResult` used by this educational unit.
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
};

/// Multi-step red entry for `obfuscation_crypto`.
RedResult apply(sim::World& w);

}  // namespace examples::obfuscation_crypto
