#pragma once
// Lab RED for `aim_humanization` (theme multi-step, Simulated).
#include "sim/world.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace examples::aim_humanization {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::uint32_t actor_pid = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::aim_humanization
