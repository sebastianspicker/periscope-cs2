#pragma once
// Lab RED for `silent_aim_desync` (theme multi-step, Simulated).
#include "sim/world.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace examples::silent_aim_desync {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::silent_aim_desync
