// BLUE: audio-only enemy inference residual (no LOS).

#include "blue_example.hpp"

#include <sstream>

namespace examples::sound_esp {

BlueResult detect(sim::World& w) {
  BlueResult r;
  int sound_only = 0;
  for (const auto& s : w.aim_samples) {
    if (s.position_from_sound_only) ++sound_only;
  }
  if (sound_only == 0) {
    sound_only = w.sound_inferred_without_los;
  }
  r.sound_only = sound_only;
  r.detected = w.sound_esp_active && sound_only >= 3;
  r.mitigated = r.detected;
  if (r.mitigated) {
    w.sound_esp_active = false;
    w.sound_inferred_without_los = 0;
    for (auto& s : w.aim_samples) {
      s.position_from_sound_only = false;
    }
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "sound_esp blue sound_only=" << r.sound_only
      << " detected=" << (r.detected ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::sound_esp
