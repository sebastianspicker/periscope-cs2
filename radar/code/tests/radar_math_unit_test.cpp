// radar_math_unit_test.cpp — Unit tests for shipped radar projection helpers.
// Drives real::cs2::periscope pure functions (no CS2 attach required).
#include "real/cs2/periscope_radar.hpp"
#include "real/cs2/periscope_hud.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_fails = 0;

static void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  } else {
    std::printf("OK: %s\n", msg);
  }
}

static bool near(float a, float b, float eps = 1e-3f) {
  return std::fabs(a - b) <= eps;
}

int main() {
  using namespace real::cs2::periscope;

  // 1) Edge formula (shipped helper)
  {
    const float e07 = world_edge_radius_for_cl_radar_scale(0.7f);
    expect(near(e07, 750.0f / 0.7f, 0.1f), "edge@0.7 == 750/0.7 (~1071.4)");
    const float e10 = world_edge_radius_for_cl_radar_scale(1.0f);
    expect(near(e10, 750.0f, 0.1f), "edge@1.0 == 750");
    const float eBad = world_edge_radius_for_cl_radar_scale(-1.f);
    expect(near(eBad, 750.0f / 0.7f, 0.1f), "bad cl_radar falls back to 0.7");
  }

  // 2) polar_to_overlay_blip axis remap
  {
    float sx = 0, sy = 0;
    // polar (x,y) for "ahead" at yaw0 maps to overlay (0, -1) style after normalize
    polar_to_overlay_blip(/*polar_x=*/-10.f, /*polar_y=*/0.f, /*edge=*/10.f, sx, sy);
    expect(near(sx, 0.f) && near(sy, -1.f), "ahead polar -> screen (0,-1)");
    polar_to_overlay_blip(0.f, 10.f, 10.f, sx, sy);
    // left of view at yaw0: Source +Y is left => screen_x negative
    expect(near(sx, -1.f) && near(sy, 0.f), "left polar -> screen (-1,0)");
  }

  // 3) polar_map always-centered: local at map center, enemy ahead (yaw=0, +X)
  {
    HudRadarSnapshot hud{};
    hud.isRound = true;
    hud.mapTexturePosition = {100.f, 200.f, 0.f};
    hud.mapTextureScale = 1.f;
    hud.maxVisibilitySquared = 100.f * 100.f;  // edge=100
    hud.originTexturePositionDifference = {0, 0, 0};
    hud.radarScale = 0.7f;
    hud.valid = true;

    Vector3 enemy{150.f, 200.f, 0.f};  // +50 ahead on +X, yaw=0
    bool oob = false;
    auto p = polar_map_player_to_radar(hud, enemy, /*yaw=*/0.f, /*rtts=*/1.f, &oob);
    float sx = 0, sy = 0;
    polar_to_overlay_blip(p.x, p.y, 100.f, sx, sy);
    expect(!oob, "enemy within edge not OOB");
    expect(near(sx, 0.f, 0.05f) && sy < -0.4f, "ahead enemy appears up (screen_y < 0)");
  }

  // 4) OOB clamp
  {
    HudRadarSnapshot hud{};
    hud.isRound = true;
    hud.mapTexturePosition = {0, 0, 0};
    hud.mapTextureScale = 1.f;
    hud.maxVisibilitySquared = 100.f;  // edge=10
    hud.originTexturePositionDifference = {0, 0, 0};
    hud.valid = true;
    Vector3 far{1000.f, 0.f, 0.f};
    bool oob = false;
    auto p = polar_map_player_to_radar(hud, far, 0.f, 1.f, &oob);
    float r = std::sqrt(p.x * p.x + p.y * p.y);
    expect(oob, "far enemy marked OOB");
    expect(near(r, 10.f, 0.5f), "OOB clamp near edge radius");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("ALL radar_math_unit_test PASSED\n");
  return 0;
}
