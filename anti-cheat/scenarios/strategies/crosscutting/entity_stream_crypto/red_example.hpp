#pragma once

// Lab RED for `entity_stream_crypto` on sim::World only (theme-specific multi-step scars).

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace examples::entity_stream_crypto {

struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::uint32_t actor_pid = 0;
  std::string detail;
  bool has_key = false;
};

RedResult apply(sim::World& w);

}  // namespace examples::entity_stream_crypto
