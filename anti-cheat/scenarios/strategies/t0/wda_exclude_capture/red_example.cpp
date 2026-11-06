#include "red_example.hpp"
#include <cstdio>

namespace examples::wda_exclude_capture {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game)
    return {false, steps, "game process unavailable"};

  const auto overlay = w.spawn("overlay.exe");
  if (!w.proc(overlay))
    return {false, steps, "overlay spawn failed"};

  sim::OverlayWindow ov;
  ov.owner_pid = overlay;
  ov.title = "radar_overlay";
  ov.topmost = true;
  ov.transparent = true;
  ov.stream_proof = true;
  w.add_overlay(ov);

  w.wda_exclude_updated = true;
  w.wda_exclude_count = 1;

  for (auto& o : w.overlays)
    if (o.owner_pid == overlay) o.stream_proof = true;

  const bool scar = w.wda_exclude_updated && w.wda_exclude_count >= 1;
  if (!scar)
    return {false, steps, "WDA exclude scar verification failed"};

  std::printf("[T0 wda_exclude_capture] step %d: WDA EXCLUDEFROMCAPTURE applied\n", ++steps);
  w.note("wda_exclude_capture: overlay excluded from capture");
  return {true, steps, "wda_exclude_capture: SetWindowDisplayAffinity EXCLUDEFROMCAPTURE", true, true};
}

}  // namespace examples::wda_exclude_capture
