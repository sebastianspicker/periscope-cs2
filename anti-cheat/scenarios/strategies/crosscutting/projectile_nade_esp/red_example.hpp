#pragma once
// Lab RED: grenade/projectile ESP + nade prediction product.
#include "sim/world.hpp"
#include <cstdint>
#include <string>
namespace examples::projectile_nade_esp {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  int projectiles = 0;
  int arcs = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
}  // namespace examples::projectile_nade_esp
