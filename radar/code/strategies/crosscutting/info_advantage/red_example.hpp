#pragma once

// Lab RED for `info_advantage` on sim::World only (theme-specific multi-step scars).

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace examples::info_advantage {

struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::uint32_t actor_pid = 0;
  std::string detail;
  int preaim_frames = 0;
  double score = 0;
};

RedResult apply(sim::World& w);

}  // namespace examples::info_advantage
