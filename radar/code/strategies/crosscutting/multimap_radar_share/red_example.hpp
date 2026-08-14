#pragma once
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::multimap_radar_share {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::multimap_radar_share
