// BLUE: non-player entity product + prediction is fog/interest residual class.

#include "blue_example.hpp"
#include <sstream>

namespace examples::projectile_nade_esp {

BlueResult detect(sim::World& w) {
  BlueResult r;
  r.projectiles = w.projectile_product_count;
  r.arcs = w.nade_predicted_arcs;
  r.detected = w.projectile_esp_active && r.projectiles >= 2;
  r.mitigated = r.detected && w.nade_prediction_active && r.arcs >= 1;
  if (r.mitigated) {
    w.projectile_esp_active = false;
    w.nade_prediction_active = false;
    w.projectile_product_count = 0;
    w.nade_predicted_arcs = 0;
    // Object-class fidelity cut (projectiles not full-origin replicated).
    w.apply_client_fidelity_budget(0.4f, /*enemy_budget=*/2);
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "projectile_nade_esp blue projectiles=" << r.projectiles
      << " arcs=" << r.arcs << " fog=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::projectile_nade_esp
