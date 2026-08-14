#pragma once
// Lab BLUE: object-class projectile product + prediction residual.
#include "sim/world.hpp"
#include <string>
namespace examples::projectile_nade_esp {
struct BlueResult {
  bool detected = false;
  bool mitigated = false;
  int projectiles = 0;
  int arcs = 0;
  std::string detail;
};
BlueResult detect(sim::World& w);
}  // namespace examples::projectile_nade_esp
