#pragma once
// Lab RED: RCS spray pitch-correction residual.
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::rcs_pattern {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  int samples = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::rcs_pattern
