// RED: infer enemy positions from audio without line-of-sight.

#include "red_example.hpp"

#include <sstream>

namespace examples::sound_esp {

RedResult apply(sim::World& w) {
  RedResult r;
  // Multi-step: plant aim samples that know position from sound only (no LOS/vision).
  for (int i = 0; i < 4; ++i) {
    sim::AimSample s;
    s.has_vision_on_target = false;
    s.has_audio_on_target = true;
    s.position_from_sound_only = true;
    s.aim_on_hidden_target = true;
    w.aim_samples.push_back(s);
  }
  w.sound_esp_active = true;
  w.sound_inferred_without_los = 4;
  r.sound_sources = 4;
  r.entities_visible = 4;
  r.achieved = w.sound_esp_active && w.sound_inferred_without_los >= 4;
  std::ostringstream oss;
  oss << "sound_esp red sources=" << r.sound_sources
      << " without_los=" << w.sound_inferred_without_los;
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::sound_esp
