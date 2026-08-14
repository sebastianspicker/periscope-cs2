#pragma once
// Lab RED for `input_provenance` (theme multi-step, Simulated).
#include "sim/world.hpp"
#include <cstdint>
#include <string>
#include <vector>
namespace examples::input_provenance {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
  int injected = 0;
};
RedResult apply(sim::World& w);
}  // namespace examples::input_provenance
