#pragma once
// Lab RED for `input_synthesis` (theme multi-step, Simulated).
#include "sim/world.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace examples::input_synthesis {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::uint32_t actor_pid = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::input_synthesis
