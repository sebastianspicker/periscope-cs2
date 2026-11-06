#pragma once

// Lab RED for `delayed_ban` on sim::World only (theme-specific multi-step scars).

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace examples::delayed_ban {

struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
  int ticks = 0;
  double confidence = 0;
};

RedResult apply(sim::World& w);

}  // namespace examples::delayed_ban
