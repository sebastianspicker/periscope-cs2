// RED: multi-subtype sound visualizer product (reload/scope/bomb-beep/footstep).
// Each subtype is a distinct panel-class scar blue can score independently.

#include "red_example.hpp"
#include <sstream>

namespace examples::sound_viz_subtypes {

RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("sound-panels.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);

  w.sound_viz_subtypes_active = true;
  w.sound_viz_reload = 3;
  w.sound_viz_scope = 2;
  w.sound_viz_bomb_beep = 4;
  w.sound_viz_footstep = 5;
  // Independent panel hits (product polish — not one catalog ID per panel).
  w.sound_panel_reload_hit = w.sound_viz_reload > 0;
  w.sound_panel_scope_hit = w.sound_viz_scope > 0;
  w.sound_panel_bomb_beep_hit = w.sound_viz_bomb_beep > 0;
  w.sound_panel_footstep_hit = w.sound_viz_footstep > 0;
  // Distinct from generic sound ESP residual class (38).
  w.sound_esp_active = false;

  r.achieved = w.sound_viz_subtypes_active && w.sound_panel_reload_hit &&
               w.sound_panel_scope_hit && w.sound_panel_bomb_beep_hit &&
               w.sound_panel_footstep_hit;
  std::ostringstream oss;
  oss << "sound_viz_subtypes red reload=" << w.sound_viz_reload
      << " scope=" << w.sound_viz_scope
      << " bomb_beep=" << w.sound_viz_bomb_beep
      << " footstep=" << w.sound_viz_footstep
      << " panels=reload,scope,bomb_beep,footstep"
      << " (not generic sound_esp)";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::sound_viz_subtypes
