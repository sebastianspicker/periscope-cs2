// BLUE: score each sound-panel subtype independently (multi-reason, not one bool).

#include "blue_example.hpp"
#include <sstream>

namespace examples::sound_viz_subtypes {

BlueResult detect(sim::World& w) {
  BlueResult r;
  // Independent scoring paths — each subtype is its own residual reason.
  r.hit_reload =
      w.sound_panel_reload_hit || w.sound_viz_reload > 0;
  r.hit_scope = w.sound_panel_scope_hit || w.sound_viz_scope > 0;
  r.hit_bomb_beep =
      w.sound_panel_bomb_beep_hit || w.sound_viz_bomb_beep > 0;
  r.hit_footstep =
      w.sound_panel_footstep_hit || w.sound_viz_footstep > 0;

  r.independent_hits = (r.hit_reload ? 1 : 0) + (r.hit_scope ? 1 : 0) +
                       (r.hit_bomb_beep ? 1 : 0) + (r.hit_footstep ? 1 : 0);

  r.detected = w.sound_viz_subtypes_active && r.independent_hits >= 2;
  // Mitigate only when multi-subtype product is clear (not single panel UI).
  r.mitigated = r.independent_hits >= 3;
  if (r.mitigated) {
    w.sound_viz_subtypes_active = false;
    w.sound_viz_reload = w.sound_viz_scope = w.sound_viz_bomb_beep =
        w.sound_viz_footstep = 0;
    w.sound_panel_reload_hit = w.sound_panel_scope_hit =
        w.sound_panel_bomb_beep_hit = w.sound_panel_footstep_hit = false;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "sound_viz_subtypes blue independent_hits=" << r.independent_hits
      << " reload=" << (r.hit_reload ? 1 : 0)
      << " scope=" << (r.hit_scope ? 1 : 0)
      << " bomb_beep=" << (r.hit_bomb_beep ? 1 : 0)
      << " footstep=" << (r.hit_footstep ? 1 : 0)
      << " finer_than_sound_esp_38=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::sound_viz_subtypes
