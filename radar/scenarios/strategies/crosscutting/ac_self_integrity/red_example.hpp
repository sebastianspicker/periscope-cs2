#pragma once
// Lab RED for `ac_self_integrity` (theme multi-step, Simulated).
#include "sim/world.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace examples::ac_self_integrity {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
  int dirty = 0;
};
RedResult apply(sim::World& w);
}  // namespace examples::ac_self_integrity
