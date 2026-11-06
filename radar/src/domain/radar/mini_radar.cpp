// mini_radar.cpp — R12 view-relative mini-radar math. See mini_radar.hpp.

#include "radar/mini_radar.hpp"

#include <algorithm>

namespace radar_domain {

void rotate_point(float& x, float& y, float center_x, float center_y,
                  float angle_deg) {
  const float radians = angle_deg * 3.14159265358979323846f / 180.0f;
  const float s = std::sin(radians);
  const float c = std::cos(radians);
  const float dx = x - center_x;
  const float dy = y - center_y;
  x = center_x + dx * c - dy * s;
  y = center_y + dx * s + dy * c;
}

void project_mini_radar(const WorldPosition& local,
                        const WorldPosition& enemy, float local_yaw_deg,
                        const MiniRadarConfig& config, float& out_x,
                        float& out_y, bool& clamped) {
  const float width = std::isfinite(config.width) && config.width > 0.0f
                          ? config.width
                          : 150.0f;
  const float height = std::isfinite(config.height) && config.height > 0.0f
                           ? config.height
                           : 150.0f;
  const float scale = std::isfinite(config.scale) && config.scale > 0.0f
                          ? config.scale
                          : 1.0f;
  const float center_x = width / 2.0f;
  const float center_y = height / 2.0f;

  // Enemy offset relative to the local player, scaled to widget pixels.
  float x = (local.x - enemy.x) / scale;
  float y = (local.y - enemy.y) / scale;

  // Prevent inverted X axis position (miata).
  x *= -1.0f;

  // Ensure position starts at center.
  x += center_x;
  y += center_y;

  // Rotate the point based on our yaw.
  rotate_point(x, y, center_x, center_y, local_yaw_deg - 90.0f);

  // Prevent the marker from going out of bounds.
  float min_x = config.margin_x;
  float max_x = width - config.clamp_shrink_x;
  float min_y = config.margin_y;
  float max_y = height - config.clamp_shrink_y;
  if (max_x < min_x) {
    std::swap(min_x, max_x);
  }
  if (max_y < min_y) {
    std::swap(min_y, max_y);
  }
  const float raw_x = x;
  const float raw_y = y;
  if (x < min_x) {
    x = min_x;
  } else if (x > max_x) {
    x = max_x;
  }
  if (y < min_y) {
    y = min_y;
  } else if (y > max_y) {
    y = max_y;
  }
  clamped = (x != raw_x) || (y != raw_y);
  out_x = x;
  out_y = y;
}

}  // namespace radar_domain
