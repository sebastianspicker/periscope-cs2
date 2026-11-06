#pragma once
#include "cs2/entities.hpp"
#include <algorithm>
#include <cmath>

namespace cs2 {

struct HudRadarState {
  bool is_round_active = false;
  bool is_square = false;
  Vector3 map_texture_position{};
  float visibility_size_max = 0.f;
  float visibility_size = 0.f;
  float map_texture_scale_stored = 0.f;  // game stores 1.0/actual
  float max_visibility_sq = 0.f;
  Vector3 origin_tex_diff{};
  float radar_scale = 0.7f;
  bool valid = false;
};

struct RadarLayout {
  float radar_to_texture_scale = 1.f;
  float yaw_radians = 0.f;
  float edge_radius = 0.f;
};

struct RadarPoint {
  Vector2 position{};
  bool out_of_bounds = false;
};

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kDefaultClRadarScale = 0.7f;
inline constexpr float kWorldEdgeBase = 750.0f;

inline constexpr float deg_to_rad(float deg) {
  return deg * kPi / 180.0f;
}

inline constexpr float rad_to_deg(float rad) {
  return rad * 180.0f / kPi;
}

// World-space radar edge radius at given cl_radar_scale (Source-style).
// edge = 750 / cl_radar_scale  (≈1071.4 at default 0.7).
inline float world_edge_radius_for_cl_radar_scale(float cl_radar_scale) {
  float z = cl_radar_scale;
  if (!(z >= 0.25f) || !(z <= 1.05f)) z = kDefaultClRadarScale;
  return kWorldEdgeBase / z;
}

// Periscope-style polar map: world -> radar plane with round OOB clamp.
// yaw_radians should already include the +90° HUD orientation fix when
// produced by compute_layout(); pass raw eye yaw through compute_layout.
inline RadarPoint translate_to_radar_position(
    const HudRadarState& hud,
    Vector3 world_pos,
    float yaw_radians,
    float radar_to_texture_scale) {
  float dx = world_pos.x - hud.map_texture_position.x;
  float dy = hud.map_texture_position.y - world_pos.y;

  float px = dx * radar_to_texture_scale - hud.origin_tex_diff.x;
  float py = dy * radar_to_texture_scale - hud.origin_tex_diff.y;
  float dist_sq = px * px + py * py;
  const float max_vis = hud.max_visibility_sq > 0.f
      ? hud.max_visibility_sq
      : (hud.visibility_size_max * hud.visibility_size_max);
  bool out_of_bounds = hud.is_round_active && max_vis > 0.f && dist_sq >= max_vis;

  float rx, ry;
  if (hud.is_round_active) {
    float c = std::cos(yaw_radians);
    float s = std::sin(yaw_radians);
    if (out_of_bounds && dist_sq > 0.0f) {
      float edge = std::sqrt(max_vis / dist_sq);
      if (hud.is_square) {
        // Rectangular radar: clamp each axis independently to the edge box.
        const float half = std::sqrt(max_vis);
        float cx = std::clamp(px, -half, half);
        float cy = std::clamp(py, -half, half);
        rx = -cx * c + cy * s;
        ry = -cx * s - cy * c;
      } else {
        rx = -px * edge * c + py * edge * s;
        ry = -px * edge * s - py * edge * c;
      }
    } else {
      rx = -px * c + py * s;
      ry = -px * s - py * c;
    }
  } else {
    rx = px;
    ry = py;
  }
  return {{rx, ry}, out_of_bounds};
}

inline RadarLayout compute_layout(const HudRadarState& hud, float local_yaw) {
  RadarLayout lay;
  lay.yaw_radians = deg_to_rad(local_yaw) + kPi * 0.5f;
  float actual_scale = hud.map_texture_scale_stored > 0.001f
      ? (1.0f / hud.map_texture_scale_stored) : 333.0f;
  const float scale = hud.radar_scale > 0.001f ? hud.radar_scale : kDefaultClRadarScale;
  lay.radar_to_texture_scale = scale / actual_scale;
  lay.edge_radius = world_edge_radius_for_cl_radar_scale(scale);
  return lay;
}

// Normalize polar_map output to overlay blip space (-1..1).
// polar_map axes: overlay_x = -polar.y / edge, overlay_y = polar.x / edge
// so "ahead" is up (negative screen_y).
inline void polar_to_overlay_blip(float polar_x, float polar_y, float edge,
                                  float& out_screen_x, float& out_screen_y) {
  const float e = (edge > 1e-6f) ? edge : 1.f;
  out_screen_x = -polar_y / e;
  out_screen_y = polar_x / e;
}

// Full polar map helper combining layout + translate + optional overlay remap.
inline RadarPoint polar_map_player_to_radar(const HudRadarState& hud,
                                           const Vector3& world_pos,
                                           float local_yaw_deg) {
  const RadarLayout layout = compute_layout(hud, local_yaw_deg);
  return translate_to_radar_position(hud, world_pos, layout.yaw_radians,
                                     layout.radar_to_texture_scale);
}

// World-to-radar then normalize into [-1,1] overlay coordinates.
inline Vector2 world_to_overlay_blip(const HudRadarState& hud,
                                     const Vector3& world_pos,
                                     float local_yaw_deg,
                                     bool* out_of_bounds = nullptr) {
  const RadarLayout layout = compute_layout(hud, local_yaw_deg);
  const RadarPoint point = translate_to_radar_position(
      hud, world_pos, layout.yaw_radians, layout.radar_to_texture_scale);
  if (out_of_bounds) *out_of_bounds = point.out_of_bounds;
  float edge = layout.edge_radius;
  if (hud.max_visibility_sq > 0.f) edge = std::sqrt(hud.max_visibility_sq);
  float sx = 0.f, sy = 0.f;
  polar_to_overlay_blip(point.position.x, point.position.y, edge, sx, sy);
  return {sx, sy};
}

// Dot radius scaled for HUD / overlay presentation.
inline float polar_dot_radius(float radar_scale, float icon_scale_min,
                              float hud_scaling, float screen_h) {
  const float z = (radar_scale >= 0.25f && radar_scale <= 1.05f)
      ? radar_scale : kDefaultClRadarScale;
  const float base = 4.0f + (1.0f - z) * 6.0f;
  const float min_r = icon_scale_min > 0.f ? icon_scale_min : 2.0f;
  const float scale = hud_scaling > 0.f ? hud_scaling : 1.0f;
  const float h = screen_h > 0.f ? screen_h : 1080.f;
  return std::max(min_r, base * scale * (h / 1080.f));
}

}  // namespace cs2
