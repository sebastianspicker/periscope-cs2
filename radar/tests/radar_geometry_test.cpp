// radar_geometry_test.cpp — R4 + R12 + R7: projection, mini-radar, cadences.

#include "radar/intervals.hpp"
#include "radar/mini_radar.hpp"
#include "radar/projection.hpp"

#include <cstdio>

namespace {

using radar_domain::MapDefinition;
using radar_domain::MapFloor;
using radar_domain::MiniRadarConfig;
using radar_domain::ViewerDemand;
using radar_domain::WorldPosition;
using radar_domain::adaptive_publish_hz;
using radar_domain::apply_floor_calibration;
using radar_domain::clamped_interval_ms;
using radar_domain::project_mini_radar;
using radar_domain::project_to_map;
using radar_domain::select_floor;

int failures = 0;

void check(bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL %s\n", what);
    ++failures;
  }
}

MapDefinition two_floor_map() {
  MapDefinition map;
  map.map = "de_test";

  MapFloor lower;
  lower.z_min = -200.0f;
  lower.z_max = 0.0f;
  lower.origin_x = -1000.0f;
  lower.origin_y = 1000.0f;
  lower.scale = 5.0f;
  lower.image_width = 400;
  lower.image_height = 400;

  MapFloor upper = lower;
  upper.z_min = 0.0f;
  upper.z_max = 200.0f;

  map.floors = {lower, upper};
  return map;
}

void test_projection_canonical() {
  const MapDefinition map = two_floor_map();
  // worldX = originX + 0.5 * scale * imageWidth  -> normalized 0.5
  // worldY = originY - 0.25 * scale * imageHeight -> normalized 0.25
  const WorldPosition world{map.floors[0].origin_x +
                                0.5f * map.floors[0].scale *
                                    static_cast<float>(map.floors[0].image_width),
                            map.floors[0].origin_y -
                                0.25f * map.floors[0].scale *
                                    static_cast<float>(map.floors[0].image_height),
                            -100.0f};
  const auto projected = project_to_map(world, map);
  check(projected.ok, "projection succeeds");
  check(projected.floor_index == 0, "lower floor selected");
  check(!projected.floor_snapped, "z inside range, no snap");
  const float dx = projected.normalized_x - 0.5f;
  const float dy = projected.normalized_y - 0.25f;
  check(dx < 0.001f && dy < 0.001f && dx > -0.001f && dy > -0.001f,
        "canonical projection values");
}

void test_floor_selection_and_snap() {
  const MapDefinition map = two_floor_map();
  bool snapped = false;
  check(select_floor(map, -100.0f, &snapped) == 0 && !snapped,
        "lower floor by z");
  check(select_floor(map, 100.0f, &snapped) == 1 && !snapped,
        "upper floor by z");
  // z beyond every range snaps to the nearest floor.
  check(select_floor(map, 500.0f, &snapped) == 1 && snapped,
        "high z snaps up");
  check(select_floor(map, -500.0f, &snapped) == 0 && snapped,
        "low z snaps down");

  MapDefinition empty;
  check(select_floor(empty, 0.0f) == -1, "empty map no floor");
  const auto projected = project_to_map(WorldPosition{}, empty);
  check(!projected.ok, "empty map projection fails");
}

void test_calibration_validation() {
  MapFloor floor;
  const float quiet_nan = 0.0f / 0.0f;
  check(!apply_floor_calibration(floor, quiet_nan, 0, 1), "nan origin x");
  check(!apply_floor_calibration(floor, 0, 0, -1.0f), "negative scale");
  check(!apply_floor_calibration(floor, 0, 0, 0.0f), "zero scale");
  check(apply_floor_calibration(floor, 10.0f, 20.0f, 2.0f), "valid override");
  check(floor.origin_x == 10.0f && floor.origin_y == 20.0f &&
            floor.scale == 2.0f,
        "override applied");
  check(floor.image_width == 1024, "unchanged dimensions");
}

void test_mini_radar_center_and_clamp() {
  const MiniRadarConfig config;  // 150x150, scale 15
  float x = 0;
  float y = 0;
  bool clamped = true;

  // Same position -> exact center, never clamped.
  project_mini_radar(WorldPosition{100, 200, 0}, WorldPosition{100, 200, 0},
                     90.0f, config, x, y, clamped);
  check(!clamped, "center not clamped");
  const float dx = x - 75.0f;
  const float dy = y - 75.0f;
  check(dx < 0.001f && dy < 0.001f && dx > -0.001f && dy > -0.001f,
        "co-located enemy maps to center");

  // Distant enemy clamps into bounds.
  project_mini_radar(WorldPosition{100, 200, 0}, WorldPosition{100, 5000, 0},
                     90.0f, config, x, y, clamped);
  check(clamped, "distant enemy clamped");
  check(x >= config.margin_x && y >= config.margin_y &&
            x <= config.width - config.clamp_shrink_x &&
            y <= config.height - config.clamp_shrink_y,
        "clamped inside bounds");

  // Rotation preserves radius: two yaws, same distance from center.
  project_mini_radar(WorldPosition{0, 0, 0}, WorldPosition{150, 0, 0}, 0.0f,
                     config, x, y, clamped);
  const float radius_a = (x - 75.0f) * (x - 75.0f) + (y - 75.0f) * (y - 75.0f);
  project_mini_radar(WorldPosition{0, 0, 0}, WorldPosition{0, 150, 0}, 90.0f,
                     config, x, y, clamped);
  const float radius_b = (x - 75.0f) * (x - 75.0f) + (y - 75.0f) * (y - 75.0f);
  const float diff = radius_a > radius_b ? radius_a - radius_b
                                         : radius_b - radius_a;
  check(diff < 1.0f, "yaw rotation preserves radius");

  // Degenerate config: zero/NaN scale falls back to 1, inverted margins swap.
  MiniRadarConfig broken;
  broken.scale = 0.0f;
  broken.margin_x = 100.0f;
  broken.clamp_shrink_x = 20.0f;  // max_x = 130 before swap
  project_mini_radar(WorldPosition{0, 0, 0}, WorldPosition{10, 0, 0}, 90.0f,
                     broken, x, y, clamped);
  check(std::isfinite(x) && std::isfinite(y), "zero scale still finite");
  check(x >= 80.0f && x <= 100.0f, "inverted margin swapped into order");
}

void test_adaptive_rates_and_clamps() {
  check(adaptive_publish_hz(ViewerDemand::None) == 4, "no viewer 4 Hz");
  check(adaptive_publish_hz(ViewerDemand::Background) == 10,
        "background 10 Hz");
  check(adaptive_publish_hz(ViewerDemand::Active) == 20, "active 20 Hz");

  check(clamped_interval_ms(5) == radar_domain::kWebRadarMinIntervalMs,
        "below floor clamps up");
  check(clamped_interval_ms(5000) == radar_domain::kWebRadarMaxIntervalMs,
        "above ceiling clamps down");
  check(clamped_interval_ms(100) == 100, "in-band unchanged");
}

}  // namespace

int main() {
  test_projection_canonical();
  test_floor_selection_and_snap();
  test_calibration_validation();
  test_mini_radar_center_and_clamp();
  test_adaptive_rates_and_clamps();

  if (failures != 0) {
    std::printf("%d geometry check(s) failed\n", failures);
    return 1;
  }
  std::printf("radar_geometry_test: all checks passed\n");
  return 0;
}
