// projection.cpp: R4 fixed north-up map projection. See projection.hpp.

#include "radar/projection.hpp"

#include <cmath>

namespace radar_domain {
namespace {

bool floor_usable(const MapFloor& floor) {
  return std::isfinite(floor.origin_x) && std::isfinite(floor.origin_y) &&
         std::isfinite(floor.scale) && floor.scale > 0.0f &&
         floor.image_width > 0 && floor.image_height > 0;
}

float distance_to_range(float z, float z_min, float z_max) {
  if (z < z_min) {
    return z_min - z;
  }
  if (z > z_max) {
    return z - z_max;
  }
  return 0.0f;
}

}  // namespace

int select_floor(const MapDefinition& map, float z, bool* snapped) {
  if (snapped != nullptr) {
    *snapped = false;
  }
  if (map.floors.empty()) {
    return -1;
  }
  for (std::size_t i = 0; i < map.floors.size(); ++i) {
    const MapFloor& floor = map.floors[i];
    if (z >= floor.z_min && z <= floor.z_max) {
      return static_cast<int>(i);
    }
  }
  // Outside every known range: pick the nearest floor so markers never
  // silently vanish (tiansongyu floor policy).
  int best = 0;
  float best_distance = 0.0f;
  for (std::size_t i = 0; i < map.floors.size(); ++i) {
    const MapFloor& floor = map.floors[i];
    const float distance = distance_to_range(z, floor.z_min, floor.z_max);
    if (i == 0 || distance < best_distance) {
      best = static_cast<int>(i);
      best_distance = distance;
    }
  }
  if (snapped != nullptr) {
    *snapped = true;
  }
  return best;
}

bool apply_floor_calibration(MapFloor& floor, float origin_x, float origin_y,
                             float scale) {
  if (!std::isfinite(origin_x) || !std::isfinite(origin_y) ||
      !std::isfinite(scale) || scale <= 0.0f) {
    return false;
  }
  floor.origin_x = origin_x;
  floor.origin_y = origin_y;
  floor.scale = scale;
  return true;
}

ProjectedPoint project_to_map(const WorldPosition& world,
                              const MapDefinition& map) {
  ProjectedPoint result;
  int floor_index = select_floor(map, world.z, &result.floor_snapped);
  if (floor_index < 0) {
    return result;
  }
  const MapFloor& floor = map.floors[static_cast<std::size_t>(floor_index)];
  if (!floor_usable(floor)) {
    return result;
  }

  // Canonical Source overview projection:
  //   pixelX = (worldX - originX) / scale
  //   pixelY = (originY - worldY) / scale
  const float pixel_x = (world.x - floor.origin_x) / floor.scale;
  const float pixel_y = (floor.origin_y - world.y) / floor.scale;
  result.normalized_x = pixel_x / static_cast<float>(floor.image_width);
  result.normalized_y = pixel_y / static_cast<float>(floor.image_height);
  result.floor_index = floor_index;
  result.ok = true;
  return result;
}

}  // namespace radar_domain
