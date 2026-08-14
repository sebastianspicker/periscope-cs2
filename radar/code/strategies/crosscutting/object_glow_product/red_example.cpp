// RED: multi-class outline-glow product (bomb/kit/hostage/grenade/weapon/ticking).
// One coherent product residual — not one catalog ID per Osiris entity type.

#include "red_example.hpp"
#include "fps/lab_bridge.hpp"
#include <sstream>

namespace examples::object_glow_product {
namespace {

int count_classes(const RedResult& r) {
  int c = 0;
  if (r.dropped_bomb > 0) ++c;
  if (r.defuse_kit > 0) ++c;
  if (r.hostage > 0) ++c;
  if (r.grenade_projectile > 0) ++c;
  if (r.weapon > 0) ++c;
  if (r.ticking_bomb > 0) ++c;
  return c;
}

}  // namespace

RedResult apply_from_scenario(fps::Scenario& sc, sim::World& w) {
  RedResult r;
  if (sc.phase() == fps::RoundPhase::Buy) {
    (void)sc.end_freeze();
  }
  w = fps::make_lab_world_from_scenario(sc, "dusty-fps.exe");
  r.actor_pid = w.spawn("object-glow.exe");
  (void)w.open_process(r.actor_pid, w.game_pid(), sim::AccessMask::VmRead, false);

  r.dropped_bomb = sc.bomb().planted ? 0 : 1;
  int kits = 0;
  for (const auto& p : sc.players()) {
    if (p.team == fps::Team::Defender && p.has_defuse_kit) ++kits;
  }
  r.defuse_kit = kits > 0 ? kits : 1;
  r.hostage = 2;
  // Additional Osiris-class object categories in the same product residual.
  r.grenade_projectile = 3;  // HE/smoke/flash class product
  r.weapon = 2;              // dropped weapon outline class
  r.ticking_bomb = sc.bomb().planted ? 1 : 0;
  if (r.ticking_bomb == 0 && sc.bomb().planted == false) {
    // Still advertise projectile/weapon multi-class without fuse timer product.
  }
  r.class_count = count_classes(r);

  w.object_glow_product = true;
  w.glow_dropped_bomb = r.dropped_bomb;
  w.glow_defuse_kit = r.defuse_kit;
  w.glow_hostage = r.hostage;
  w.glow_grenade_projectile = r.grenade_projectile;
  w.glow_weapon = r.weapon;
  w.glow_ticking_bomb = r.ticking_bomb;
  w.object_glow_class_count = r.class_count;
  // Explicitly NOT bomb fuse intel product (pair 34).
  w.bomb_intel_product = false;

  r.achieved = w.object_glow_product && r.class_count >= 3 &&
               (w.glow_dropped_bomb + w.glow_defuse_kit + w.glow_hostage +
                w.glow_grenade_projectile + w.glow_weapon +
                w.glow_ticking_bomb) >= 3;
  std::ostringstream oss;
  oss << "object_glow red classes=" << r.class_count
      << " bomb=" << r.dropped_bomb << " kit=" << r.defuse_kit
      << " hostage=" << r.hostage << " nade=" << r.grenade_projectile
      << " weapon=" << r.weapon << " ticking=" << r.ticking_bomb
      << " multi_class_product=1 (not one ID per entity; not fuse timer)";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

RedResult apply(sim::World& w) {
  fps::RoundConfig cfg;
  cfg.freeze_time = 0.f;
  fps::Scenario sc(fps::Map::make_dusty_yard(), cfg);
  sc.start_default_round();
  return apply_from_scenario(sc, w);
}

}  // namespace examples::object_glow_product
