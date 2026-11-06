#pragma once
// Lab BLUE: audio-only enemy inference residual (no LOS).
#include "sim/world.hpp"
#include <string>
namespace examples::sound_esp {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int sound_only = 0;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::sound_esp
