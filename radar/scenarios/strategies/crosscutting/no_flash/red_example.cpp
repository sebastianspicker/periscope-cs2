// RED: force flash alpha to zero (client FX strip) — not player ESP.

#include "red_example.hpp"

#include <cstdio>
#include <sstream>

namespace examples::no_flash {

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

  // Phase 1: no-flash actor.
  r.actor_pid = w.spawn("no-flash.exe");
  if (!w.proc(r.actor_pid)) {
    r.detail = "actor spawn verification failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 2: internal-style helper module inject into game.
  (void)w.inject_module(game_pid,
                        sim::Module{"noflash.hlp", game->base + 0x5000, 0x1000,
                                    true, false, "patched", false, false, false},
                        /*manual_map=*/false);
  bool helper_present = false;
  for (const auto& m : game->modules) {
    if (m.name.find("noflash") != std::string::npos || m.text_hash == "patched") {
      helper_present = true;
      break;
    }
  }
  if (!helper_present) {
    r.detail = "noflash helper inject verification failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  // Phase 3: force flash alpha to zero (FX strip).
  w.no_flash_active = true;
  w.flash_alpha_forced = 0.f;
  if (!(w.no_flash_active && w.flash_alpha_forced < 0.1f)) {
    r.detail = "flash alpha strip plant failed";
    r.steps = steps;
    return r;
  }
  ++steps;

  r.achieved = true;
  r.steps = steps;
  std::ostringstream oss;
  oss << "no_flash red alpha=" << w.flash_alpha_forced
      << " helper=1 strip=1 steps=" << steps;
  r.detail = oss.str();
  w.note(r.detail);
  std::printf("[no_flash] RED achieved=true steps=%d\n", steps);
  return r;
}

}  // namespace examples::no_flash
