// RED: multi-step plant-feasibility + defuse-window HUD intel (≠ fuse timer).

#include "red_example.hpp"
#include "fps/lab_bridge.hpp"
#include <sstream>

namespace examples::plant_defuse_hud {

RedResult apply_from_scenario(fps::Scenario& sc, sim::World& w) {
  RedResult r;
  // Phase 1: advance scenario into live clock residual.
  if (sc.phase() == fps::RoundPhase::Buy) {
    (void)sc.end_freeze();
  }
  ++r.steps;

  // Scenario::clock() is *remaining* time (counts down).
  const float time_remaining = sc.clock();
  const float plant_need = sc.config().plant_time;
  r.plant_feasible = time_remaining > plant_need + 1.f;
  r.time_remaining = time_remaining;

  // Phase 2: lab world + external reader actor with game VM_READ.
  w = fps::make_lab_world_from_scenario(sc, "dusty-fps.exe");
  r.actor_pid = w.spawn("plant-hud.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  ++r.steps;

  // Phase 3: plant-feasibility HUD scar (timing window before round end).
  w.plant_hud_alert_active = true;
  w.plant_feasible_before_round_end = r.plant_feasible;
  w.plant_time_remaining_known = r.time_remaining;
  ++r.steps;

  // Phase 4: defuse-window alert scar (kit vs no-kit hold knowledge).
  w.defuse_window_alert_active = true;
  r.defuse_window = true;
  ++r.steps;

  // Distinct from bomb fuse product (pair 34).
  w.bomb_intel_product = false;
  w.bomb_fuse_known = -1.f;

  r.achieved = w.plant_hud_alert_active && w.defuse_window_alert_active &&
               w.plant_time_remaining_known >= 0.f && r.steps >= 2;
  std::ostringstream oss;
  oss << "plant_defuse_hud red steps=" << r.steps
      << " plant_feasible=" << (r.plant_feasible ? 1 : 0)
      << " t_left=" << r.time_remaining
      << " defuse_window=1 (not fuse timer)";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

RedResult apply(sim::World& w) {
  fps::RoundConfig cfg;
  cfg.freeze_time = 0.f;
  fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
  sc.start_default_round();
  (void)sc.end_freeze();
  return apply_from_scenario(sc, w);
}

}  // namespace examples::plant_defuse_hud
