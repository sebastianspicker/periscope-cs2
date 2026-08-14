#pragma once
// Lab RED: multi-step plant-feasibility / defuse-window HUD alerts.
#include "sim/world.hpp"
#include "fps/scenario.hpp"
#include <cstdint>
#include <string>
namespace examples::plant_defuse_hud {
struct RedResult {
  bool achieved = false;
  std::uint32_t actor_pid = 0;
  bool plant_feasible = false;
  bool defuse_window = false;
  float time_remaining = -1.f;
  int steps = 0;
  std::string detail;
};
RedResult apply(sim::World& w);
RedResult apply_from_scenario(fps::Scenario& sc, sim::World& w);
}  // namespace examples::plant_defuse_hud
