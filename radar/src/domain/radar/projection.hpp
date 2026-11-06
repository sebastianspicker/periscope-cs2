// projection.hpp — R4: fixed north-up map projection (Source overview coords).
//
// Adapted from the research ledger (docs/ADAPTATION_LEDGER.md R4):
//   - tiansongyu/cs2_cheat web-radar design (MIT): canonical projection,
//     multi-floor selection by player Z with nearest-floor snapping.
//   - cs2-dma Radar/Calibration (Apache-2.0): per-map calibration overrides.
//
// Recorded decision: fixed north-up only, no rotating/circular mode. The same
// projection source feeds the local overlay, the web radar (R5) and the relay
// (R6) so the coordinate systems cannot drift.

#pragma once

#include <string>
#include <vector>

#include "radar/snapshot.hpp"

namespace radar_domain {

// One floor layer of a map, in Source overview coordinates.
struct MapFloor {
  float z_min = 0.0f;                 // inclusive lower Z bound
  float z_max = 0.0f;                 // inclusive upper Z bound
  float origin_x = 0.0f;              // world X at image left edge
  float origin_y = 0.0f;              // world Y at image top edge
  float scale = 1.0f;                 // world units per image pixel
  int image_width = 1024;             // radar image size in pixels
  int image_height = 1024;
};

// Map definition with floors sorted by Z (ascending). Floor selection picks
// the first floor whose [z_min, z_max] contains the player's Z; when Z falls
// outside every known range the nearest floor (by distance to the range) is
// chosen so markers never silently vanish.
struct MapDefinition {
  std::string map;
  std::vector<MapFloor> floors;  // ascending by z_min
};

struct ProjectedPoint {
  bool ok = false;
  float normalized_x = 0.0f;  // 0..1 across the floor image
  float normalized_y = 0.0f;
  int floor_index = 0;
  bool floor_snapped = false;  // true when Z was outside every floor range
};

// Projects a world position onto the map. Returns ok=false when the map has
// no usable floor (empty, non-positive scale, or non-positive dimensions).
ProjectedPoint project_to_map(const WorldPosition& world,
                              const MapDefinition& map);

// Chooses the floor index for a Z value: containing floor first, then nearest
// by distance to the floor's Z range. Returns -1 for an empty map.
int select_floor(const MapDefinition& map, float z, bool* snapped = nullptr);

// Applies a per-map calibration override (cs2-dma Radar/Calibration pattern):
// replaces the floor's origin/scale before projection. Bad values (non-finite,
// non-positive scale) are rejected and the floor is left unchanged; the
// function reports whether anything was applied.
bool apply_floor_calibration(MapFloor& floor, float origin_x, float origin_y,
                             float scale);

}  // namespace radar_domain
