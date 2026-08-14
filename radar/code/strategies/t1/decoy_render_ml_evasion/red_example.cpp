#include "red_example.hpp"
#include <cstdio>

namespace examples::decoy_render_ml_evasion {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  if (!w.proc(game_pid) || !w.proc(game_pid)->is_game)
    return {false, steps, "game unavailable"};

  const auto decoy = w.spawn("decoy-render.exe");
  if (!w.proc(decoy))
    return {false, steps, "decoy spawn failed"};

  w.decoy_render_active = true;
  w.decoy_frame_count = 30;
  w.ml_confusion_active = true;
  w.ml_decoy_patterns_generated = 15;

  const bool scar = w.decoy_render_active && w.ml_confusion_active && w.ml_decoy_patterns_generated >= 10;
  if (!scar)
    return {false, steps, "decoy render ML scar failed"};

  std::printf("[T1 decoy_render_ml_evasion] step %d: %d decoy frames, %d ML patterns\n", ++steps, w.decoy_frame_count, w.ml_decoy_patterns_generated);
  w.note("decoy_render_ml_evasion: ML behavioral confusion via decoy render");
  return {true, steps, "decoy_render_ml_evasion: ML confusion active", w.decoy_frame_count, w.ml_decoy_patterns_generated, true};
}

}  // namespace examples::decoy_render_ml_evasion
