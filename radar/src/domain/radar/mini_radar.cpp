// mini_radar.cpp — R12 view-relative mini-radar math. See mini_radar.hpp.

#include "radar/mini_radar.hpp"

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
  const float center_x = config.width / 2.0f;
  const float center_y = config.height / 2.0f;

  // Enemy offset relative to the local player, scaled to widget pixels.
  float x = (local.x - enemy.x) / config.scale;
  float y = (local.y - enemy.y) / config.scale;

  // Prevent inverted X axis position (miata).
  x *= -1.0f;

  // Ensure position starts at center.
  x += center_x;
  y += center_y;

  // Rotate the point based on our yaw.
  rotate_point(x, y, center_x, center_y, local_yaw_deg - 90.0f);

  // Prevent the marker from going out of bounds.
  const float min_x = config.margin_x;
  const float max_x = config.width - config.clamp_shrink_x;
  const float min_y = config.margin_y;
  const float max_y = config.height - config.clamp_shrink_y;
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
