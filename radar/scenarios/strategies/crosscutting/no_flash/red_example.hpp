#pragma once
// Lab RED: no-flash / client FX strip residual.
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::no_flash {
struct RedResult {
  bool achieved = false;
  int steps = 0;
  std::uint32_t actor_pid = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::no_flash
