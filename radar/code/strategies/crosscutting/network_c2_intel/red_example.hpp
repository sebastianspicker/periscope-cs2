#pragma once
// Lab RED for `network_c2_intel` (theme multi-step, Simulated).
#include "sim/world.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace examples::network_c2_intel {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
  int intel_nets = 0;
};
RedResult apply(sim::World& w);
}  // namespace examples::network_c2_intel
