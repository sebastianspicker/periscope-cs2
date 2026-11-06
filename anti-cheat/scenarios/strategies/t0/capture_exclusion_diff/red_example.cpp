#include "red_example.hpp"

#include <cstdio>
#include <utility>

namespace examples::capture_exclusion_diff {

RedResult apply(sim::World& w) {
  RedResult result;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    return {false, 0, "precondition failed: game process unavailable"};
  }

  const auto presenter = w.spawn("excluded-presenter.exe");
  if (!w.proc(presenter)) return {false, 0, "presenter spawn failed"};
  result.steps = 1;

  // Present an overlay over the game window.
  sim::OverlayWindow overlay{};
  overlay.owner_pid = presenter;
  overlay.title = "excluded-overlay";
  overlay.topmost = true;
  overlay.stream_proof = true;
  w.add_overlay(std::move(overlay));
  result.steps = 2;

  // The window is excluded from capture. The player still sees it; a
  // capture-based sensor does not.
  w.wda_excluded_from_capture = true;
  w.wda_exclude_updated = true;
  ++w.wda_exclude_count;
  w.capture_sees_overlays = false;
  w.capture_frame_diff_pixels = 0;
  result.steps = 3;

  result.achieved = w.wda_excluded_from_capture && !w.capture_sees_overlays &&
                    w.capture_frame_diff_pixels == 0;
  result.detail = "capture_exclusion_diff: excluded=1 capture_sees=0 diff_px=0";
  std::printf("[T0 capture_exclusion_diff] step %d: overlay excluded from capture\n",
              result.steps);
  w.note(result.detail);
  return result;
}

}  // namespace examples::capture_exclusion_diff
