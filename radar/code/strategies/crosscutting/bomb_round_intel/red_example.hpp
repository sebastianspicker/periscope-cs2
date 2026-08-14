#pragma once
// Lab RED: planted/defuse timer intel product beyond player XY.
#include "sim/world.hpp"
#include "fps/scenario.hpp"
#include <cstdint>
#include <string>
namespace examples::bomb_round_intel {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  bool bomb_planted = false;
  std::string site;
  float fuse_known = -1.f;
  bool defuse_known = false;
  float defuse_progress = 0.f;
  std::string detail;
};
RedResult apply(sim::World& w);
RedResult apply_from_scenario(fps::Scenario& sc, sim::World& w);
}  // namespace examples::bomb_round_intel
