#pragma once
// Lab RED: multi-step stream-proof / band-4 overlay vs capture path.
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::streamproof_overlay {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  int steps = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::streamproof_overlay
