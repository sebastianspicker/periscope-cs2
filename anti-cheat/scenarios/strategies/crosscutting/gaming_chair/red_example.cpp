#include "red_example.hpp"

#include <cstdio>

namespace examples::gaming_chair {

RedResult apply(sim::World& w, RedConfig cfg) {
  RedResult r;
  int steps = 0;

  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    r.detail = "precondition failed: game process unavailable";
    return r;
  }
  ++steps;

  // Multi-step product suite: actor then stacked features.
  r.actor_pid = w.spawn("gaming-chair.exe");
  if (!w.proc(r.actor_pid)) {
    r.detail = "actor spawn verification failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  if (cfg.proxy_hijack) {
    r.features.push_back("proxy");
    r.active++;
    w.handle_proxy_active = true;
    w.handle_proxy_consumer_pid = r.actor_pid;
    w.remote_read_ops += 250;
    w.remote_read_bytes += 1'500'000;
    w.note("proxy_hijack enabled");
    ++steps;
  }
  if (cfg.band4) {
    r.features.push_back("band4");
    r.active++;
    w.schema_cache_active = true;
    w.schema_remote_update = true;
    w.schema_saas_product = true;
    ++steps;
  }
  if (cfg.esp) {
    r.features.push_back("esp");
    r.active++;
    w.object_glow_product = true;
    w.glow_weapon = 1;
    w.object_glow_class_count = 1;
    ++steps;
  }
  if (cfg.aimbot) {
    r.features.push_back("aimbot");
    r.active++;
    w.triggerbot_active = true;
    ++steps;
  }

  r.achieved = r.active >= 2 && w.remote_read_ops > 200;
  r.steps = steps;
  r.detail = "gaming_chair red features=" + std::to_string(r.active) +
             " reads=" + std::to_string(w.remote_read_ops) +
             " steps=" + std::to_string(steps);
  w.note(r.detail);
  std::printf("[gaming_chair] RED achieved=%s active=%d steps=%d\n",
              r.achieved ? "true" : "false", r.active, steps);
  return r;
}

}  // namespace examples::gaming_chair
