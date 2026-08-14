// RED: projectile ESP product + predicted nade arcs (not player XY alone).

#include "red_example.hpp"
#include <sstream>

namespace examples::projectile_nade_esp {

RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("nade-esp.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);
  w.projectile_esp_active = true;
  w.projectile_product_count = 4;  // HE/smoke/flash/molotov class product
  w.nade_prediction_active = true;
  w.nade_predicted_arcs = 3;
  r.projectiles = w.projectile_product_count;
  r.arcs = w.nade_predicted_arcs;
  r.achieved = w.projectile_esp_active && r.projectiles >= 2 &&
               w.nade_prediction_active && r.arcs >= 1;
  std::ostringstream oss;
  oss << "projectile_nade_esp red projectiles=" << r.projectiles
      << " arcs=" << r.arcs;
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::projectile_nade_esp
