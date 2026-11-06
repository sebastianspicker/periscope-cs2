#pragma once
// Lab RED: observer/spectator list feed residual.
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::spectator_feed {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  int spectator_count = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::spectator_feed
