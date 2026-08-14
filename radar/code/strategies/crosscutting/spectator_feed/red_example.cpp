// RED: spectator list + delayed enemy origin without local LOS/RPM product.

#include "red_example.hpp"

#include <sstream>

namespace examples::spectator_feed {

RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("spec-list.exe");
  // Optional: soft handle (not required for pure spectator second-client story).
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::Query, false);

  w.spectator_feed_active = true;
  w.spectator_has_delayed_enemy_origin = true;
  w.spectator_target_controller = w.game_pid();  // lab: watch local/game controller
  w.spectator_count = 3;  // three observers watching local pawn (product)
  r.spectator_count = w.spectator_count;

  r.achieved = w.spectator_feed_active && w.spectator_has_delayed_enemy_origin &&
               w.spectator_count >= 1;
  std::ostringstream oss;
  oss << "spectator_feed red count=" << r.spectator_count
      << " delayed_origin=1 target=" << w.spectator_target_controller;
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::spectator_feed
