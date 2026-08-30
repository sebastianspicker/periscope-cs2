#pragma once
// Lab RED: multi-class Osiris-style object glow product (not one ID per entity).
#include "sim/world.hpp"
#include "fps/scenario.hpp"
#include <cstdint>
#include <string>
namespace examples::object_glow_product {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  int dropped_bomb = 0;
  int defuse_kit = 0;
  int hostage = 0;
  int grenade_projectile = 0;
  int weapon = 0;
  int ticking_bomb = 0;
  int class_count = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
RedResult apply_from_scenario(fps::Scenario& sc, sim::World& w);
}  // namespace examples::object_glow_product
