#pragma once
// Lab BLUE: independently score sound visualizer subtype classes.
#include "sim/world.hpp"
#include <string>
namespace examples::sound_viz_subtypes {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  bool hit_reload = false;
  bool hit_scope = false;
  bool hit_bomb_beep = false;
  bool hit_footstep = false;
  int independent_hits = 0;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::sound_viz_subtypes
