#pragma once

// RED: CS-inspired dusty_yard round → lab_bridge World → RPM entity product.
// Scenario-derived positions (teams/alive/plant), not static make_arena table alone.

#include "sim/world.hpp"
#include "fps/scenario.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "ac/types.hpp"

namespace examples::cs_round_radar {

struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  int entity_count = 0;
  int attackers_alive = 0;
  int defenders_alive = 0;
  bool bomb_planted = false;
  std::string site;
  std::vector<ac::EntitySnapshot> entities;
  std::string detail;
};

// Build planted CS round, bridge into World, open VmRead, pull entity table.
// If w is empty arena, replaces with scenario world contents via sync.
RedResult apply(sim::World& w);

// Same but returns the scenario used (tests assert site/team layout).
RedResult apply_from_scenario(fps::Scenario& sc, sim::World& w);

}  // namespace examples::cs_round_radar
