// map.cpp — dusty_yard layout: named areas + bomb sites A/B with plant radii.

#include "fps/map.hpp"

namespace fps {

Map Map::make_dusty_yard() {
  Map m;
  m.name = "dusty_yard";
  // Layout (top-down, y up unused):
  //   CT spawn (0,0,80) ---- mid (0,0,40) ---- T spawn (0,0,0)
  //        \ site A (-30,0,50)    site B (30,0,50) /
  // Areas listed so spawns/sites resolve before the larger mid disk when
  // points sit near boundaries (first-match area_at).
  m.areas = {
      {AreaId::TSpawn, "t_spawn", {0.f, 0.f, 0.f}, 12.f},
      {AreaId::CTSpawn, "ct_spawn", {0.f, 0.f, 80.f}, 12.f},
      {AreaId::SiteA, "site_a", {-30.f, 0.f, 50.f}, 14.f},
      {AreaId::SiteB, "site_b", {30.f, 0.f, 50.f}, 14.f},
      {AreaId::Connector, "connector", {-15.f, 0.f, 30.f}, 10.f},
      {AreaId::Mid, "mid", {0.f, 0.f, 40.f}, 18.f},
  };
  m.sites = {
      {AreaId::SiteA, "A", {-30.f, 0.f, 50.f}, 4.f},
      {AreaId::SiteB, "B", {30.f, 0.f, 50.f}, 4.f},
  };
  return m;
}

const Area* Map::find_area(AreaId id) const {
  for (const auto& a : areas) {
    if (a.id == id) {
      return &a;
    }
  }
  return nullptr;
}

const BombSite* Map::find_site(const std::string& name) const {
  for (const auto& s : sites) {
    if (s.name == name) {
      return &s;
    }
  }
  return nullptr;
}

const Area* Map::area_at(const ac::Vec3& p) const {
  // Prefer the tightest containing disk when areas overlap (lab clarity).
  const Area* best = nullptr;
  float best_r2 = 0.f;
  for (const auto& a : areas) {
    const float r2 = a.radius * a.radius;
    if (dist2(p, a.center) <= r2) {
      if (!best || r2 < best_r2) {
        best = &a;
        best_r2 = r2;
      }
    }
  }
  return best;
}

const BombSite* Map::site_at(const ac::Vec3& p) const {
  for (const auto& s : sites) {
    if (dist2(p, s.plant_origin) <= s.plant_radius * s.plant_radius) {
      return &s;
    }
  }
  return nullptr;
}

}  // namespace fps
