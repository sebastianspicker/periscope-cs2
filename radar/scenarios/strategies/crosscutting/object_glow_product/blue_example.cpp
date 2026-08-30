// BLUE: multi-class object glow product without fuse-timer dependency.

#include "blue_example.hpp"
#include <sstream>

namespace examples::object_glow_product {

BlueResult detect(sim::World& w) {
  BlueResult r;
  r.objects = w.glow_dropped_bomb + w.glow_defuse_kit + w.glow_hostage +
              w.glow_grenade_projectile + w.glow_weapon + w.glow_ticking_bomb;
  int classes = 0;
  if (w.glow_dropped_bomb > 0) ++classes;
  if (w.glow_defuse_kit > 0) ++classes;
  if (w.glow_hostage > 0) ++classes;
  if (w.glow_grenade_projectile > 0) ++classes;
  if (w.glow_weapon > 0) ++classes;
  if (w.glow_ticking_bomb > 0) ++classes;
  r.class_count = classes > 0 ? classes : w.object_glow_class_count;
  r.multi_class = r.class_count >= 3;

  r.detected = w.object_glow_product && r.objects >= 2 && !w.bomb_intel_product;
  r.mitigated = r.detected && r.multi_class;
  if (r.mitigated) {
    w.object_glow_product = false;
    w.glow_dropped_bomb = 0;
    w.glow_defuse_kit = 0;
    w.glow_hostage = 0;
    w.glow_grenade_projectile = 0;
    w.glow_weapon = 0;
    w.glow_ticking_bomb = 0;
    w.object_glow_class_count = 0;
    w.apply_client_fidelity_budget(0.3f, 1);
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "object_glow blue objects=" << r.objects
      << " classes=" << r.class_count
      << " multi_class=" << (r.multi_class ? 1 : 0)
      << " not_fuse_timer=1 not_one_id_per_entity=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::object_glow_product
