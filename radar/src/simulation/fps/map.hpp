#pragma once

// Dummy FPS map for the anti-cheat lab example.
// Simple named areas + bomb sites — not a full 3D engine map.

#include "ac/types.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace fps {

enum class AreaId : std::uint8_t {
  TSpawn = 0,
  CTSpawn,
  Mid,
  Connector,
  SiteA,
  SiteB,
  Count,
};

// Lab type `Area` used by this educational unit.
struct Area {
  AreaId id = AreaId::Mid;
  std::string name;
  ac::Vec3 center{};
  float radius = 10.f;  // navigable / standable disk (lab units)
};

// Lab type `BombSite` used by this educational unit.
struct BombSite {
  AreaId area = AreaId::SiteA;
  std::string name;  // "A" / "B"
  ac::Vec3 plant_origin{};
  float plant_radius = 4.f;
};

/// Compact “Dusty Yard” layout: two sites, mid, spawns.
struct Map {
  std::string name = "dusty_yard";
  std::vector<Area> areas;
  std::vector<BombSite> sites;

  static Map make_dusty_yard();

  const Area* find_area(AreaId id) const;
  const BombSite* find_site(const std::string& name) const;
  /// Which area disk contains p (first match); null if outside all.
  const Area* area_at(const ac::Vec3& p) const;
  /// Bomb site whose plant radius contains p; null if none.
  const BombSite* site_at(const ac::Vec3& p) const;
};

inline float dist2(const ac::Vec3& a, const ac::Vec3& b) {
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;
  return dx * dx + dy * dy + dz * dz;
}

inline float dist(const ac::Vec3& a, const ac::Vec3& b) {
  return std::sqrt(dist2(a, b));
}

}  // namespace fps
