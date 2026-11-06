#include "red_example.hpp"

#include <cstdio>
#include <sstream>

namespace examples::fov_viewmodel_mod {

RedResult apply(sim::World& w) {
  RedResult r;
  int steps = 0;

  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    r.detail = "precondition failed: game process unavailable";
    return r;
  }
  ++steps;

  // Phase 1: presentation mod actor.
  r.actor_pid = w.spawn("fov-mod.exe");
  if (!w.proc(r.actor_pid)) {
    r.detail = "actor spawn verification failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 2: client FOV override (presentation residual, not ESP product).
  w.fov_mod_active = true;
  w.client_fov_override = 110.f;
  if (!(w.fov_mod_active && w.client_fov_override > 90.f)) {
    r.detail = "client FOV override plant failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 3: viewmodel FOV override (second independent presentation knob).
  w.viewmodel_fov_override = 68.f;
  if (!(w.viewmodel_fov_override > 54.f)) {
    r.detail = "viewmodel FOV override plant failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  r.achieved = w.fov_mod_active && w.client_fov_override > 90.f &&
               w.viewmodel_fov_override > 54.f;
  r.steps = steps;
  std::ostringstream oss;
  oss << "fov_viewmodel_mod red client_fov=" << w.client_fov_override
      << " viewmodel_fov=" << w.viewmodel_fov_override
      << " steps=" << steps;
  r.detail = oss.str();
  w.note(r.detail);
  std::printf("[fov_viewmodel_mod] RED achieved=%s steps=%d\n",
              r.achieved ? "true" : "false", steps);
  return r;
}

}  // namespace examples::fov_viewmodel_mod
