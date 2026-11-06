#include "red_example.hpp"
#include "t0_red/cheat_client.hpp"

#include <cstdio>

namespace examples::overlay_esp {

RedResult apply(sim::World& w) {
  RedResult result;
  int steps = 0;

  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.detail = "precondition failed: game process unavailable";
    return result;
  }
  ++steps;

  // Phase 1: attach + entity product via external RPM path.
  t0_red::CheatClient cheat(w, "esp-overlay.exe");
  cheat.attach_to_game();
  cheat.pull_entities();
  ++steps;

  // Phase 2: Steam overlay present hijack residual.
  cheat.enable_steam_overlay_hijack();
  ++steps;

  // Phase 3: second window hijack path (Discord) + radar render product.
  cheat.enable_window_hijack_overlay("Discord.exe");
  cheat.render_radar(true);
  ++steps;

  result.actor_pid = cheat.pid();
  result.achieved = cheat.last_report().entities_ok && !w.overlays.empty();
  result.detail = "overlay_esp red steam_hijack+discord_hijack entities_ok=" +
                  std::to_string(cheat.last_report().entities_ok ? 1 : 0) +
                  " overlays=" + std::to_string(w.overlays.size()) +
                  " steps=" + std::to_string(steps);
  w.note(result.detail);
  std::printf("[overlay_esp] RED achieved=%s steps=%d\n",
              result.achieved ? "true" : "false", steps);
  return result;
}

}  // namespace examples::overlay_esp
