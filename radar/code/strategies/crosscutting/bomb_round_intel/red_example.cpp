// RED: CS planted round → bomb timer / defuse progress product (not just XY).

#include "red_example.hpp"
#include "fps/lab_bridge.hpp"

#include <sstream>

namespace examples::bomb_round_intel {

RedResult apply_from_scenario(fps::Scenario& sc, sim::World& w) {
  RedResult r;
  // Buy kit during freeze/live before plant (buy blocked once BombPlanted).
  std::uint32_t defuser = 0;
  for (const auto& p : sc.players()) {
    if (p.team == fps::Team::Defender && p.alive) {
      defuser = p.id;
      break;
    }
  }
  if (defuser) {
    (void)sc.buy_defuse_kit(defuser);
  }
  if (sc.phase() == fps::RoundPhase::Buy) {
    (void)sc.end_freeze();
  }
  if (!sc.bomb().planted) {
    (void)sc.plant_instant(1, "A");
  }
  // Start a defuse so red can leak defuse progress residual.
  if (defuser && sc.bomb().planted) {
    (void)sc.move_player(defuser, sc.bomb().origin);
    (void)sc.begin_defuse(defuser);
    (void)sc.tick(1.5f);  // partial defuse progress
  }

  w = fps::make_lab_world_from_scenario(sc, "dusty-fps.exe");
  r.actor_pid = w.spawn("bomb-timer-radar.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);

  r.bomb_planted = sc.bomb().planted;
  r.site = sc.bomb().site_name;
  r.fuse_known = sc.bomb().fuse_remaining;
  r.defuse_known = sc.bomb().defuse_in_progress || sc.bomb().defuse_progress > 0.f;
  r.defuse_progress = sc.bomb().defuse_progress;

  w.bomb_intel_product = true;
  w.bomb_fuse_known = r.fuse_known;
  w.bomb_site_known = r.site;
  w.bomb_defuse_known = r.defuse_known;
  w.bomb_defuse_progress_known = r.defuse_progress;

  r.achieved = r.bomb_planted && r.fuse_known > 0.f && !r.site.empty() &&
               w.bomb_intel_product;
  std::ostringstream oss;
  oss << "bomb_round_intel red site=" << r.site
      << " fuse=" << r.fuse_known
      << " defuse_prog=" << r.defuse_progress
      << " planted=" << (r.bomb_planted ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

RedResult apply(sim::World& w) {
  fps::RoundConfig cfg;
  cfg.freeze_time = 0.f;
  fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
  sc.start_default_round();
  return apply_from_scenario(sc, w);
}

}  // namespace examples::bomb_round_intel
