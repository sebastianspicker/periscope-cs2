// mini_radar.hpp: R12: view-relative mini-radar math.
//
// Ported from staromodny/miata `src/main.cpp` (license verified per-tree;
// see NOTICE): the complete ~100-line rotation/clamp projection used for a
// local-player-centric rotating radar.
//
// Miata's math, kept verbatim in spirit:
//   screen = (local - enemy) / scale      (enemy offset relative to local)
//   screen.x *= -1                        (prevent inverted X axis)
//   screen += center                      (start at center)
//   rotate around center by (yaw - 90°)   (view-relative orientation)
//   clamp to bounds                       (keep the marker inside the widget)
//
// Deliberately presented as the *contrast* to the fixed north-up radar (R4):
// same GameSnapshot in, two different presentations out, drift-proof by
// construction.

#pragma once

#include <cmath>
#include <cstdint>

#include "radar/snapshot.hpp"

namespace radar_domain {

struct MiniRadarConfig {
  float width = 150.0f;    // widget size in pixels
  float height = 150.0f;
  float scale = 15.0f;     // world units per pixel
  float margin_x = 5.0f;   // clamp margins (miata defaults)
  float margin_y = 5.0f;
  float clamp_shrink_x = 10.0f;  // miata clamps to [5, w-10]
  float clamp_shrink_y = 10.0f;
};

// Rotates `point` around `center` by `angle_deg` degrees (screen convention,
// matching miata's rotate_point).
void rotate_point(float& x, float& y, float center_x, float center_y,
                  float angle_deg);

// Projects an enemy world position into mini-radar widget coordinates.
// Returns the clamped widget-space point; `clamped` reports whether the
// clamp engaged.
void project_mini_radar(const WorldPosition& local,
                        const WorldPosition& enemy, float local_yaw_deg,
                        const MiniRadarConfig& config, float& out_x,
                        float& out_y, bool& clamped);

}  // namespace radar_domain
