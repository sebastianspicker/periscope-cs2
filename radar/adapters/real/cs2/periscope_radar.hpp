// periscope_radar.hpp — Radar coordinate math engine.
//
// Converts world coordinates to screen radar coordinates using
// the HUD snapshot. Implements Periscope's PolarMapPlayerToRadar()
// with round/rect radar handling and multi-offset scaling.
//
// Reference: Periscope prototype/99-radar.txt lines 258-298
//            Periscope prototype/src/features/radar.cpp lines 300-450

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"
#include "real/cs2/periscope_hud.hpp"  // HudRadarSnapshot, CvarInputs

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace real::cs2::periscope {

// ====================================================================
// Coordinate transform
// ====================================================================

/// Convert world position to radar screen position.
Vector2 polar_map_player_to_radar(
    const HudRadarSnapshot& hud,
    const Vector3& worldPos,
    float yaw,
    float radarToTextureScale,
    bool* outOfBounds = nullptr) noexcept;

/// Normalize polar_map output to overlay blip space (-1..1).
/// polar_map axes differ from our draw path (screen_x=right, screen_y=-forward/up):
///   overlay_x = -polar.y / edge
///   overlay_y =  polar.x / edge
inline void polar_to_overlay_blip(float polar_x, float polar_y, float edge,
                                  float& out_screen_x, float& out_screen_y) noexcept {
    const float e = (edge > 1e-6f) ? edge : 1.f;
    out_screen_x = -polar_y / e;
    out_screen_y =  polar_x / e;
}

/// World-space radar edge radius at given cl_radar_scale (Source-style).
/// edge = 750 / cl_radar_scale  (≈1071.4 at default 0.7).
inline float world_edge_radius_for_cl_radar_scale(float cl_radar_scale) noexcept {
    float z = cl_radar_scale;
    if (!(z >= 0.25f) || !(z <= 1.05f)) z = 0.7f;
    return 750.0f / z;
}

// ====================================================================
// Screen layout computation
// ====================================================================
struct RadarLayout {
    float centerX{}, centerY{};
    float minX{}, minY{};
    float maxX{}, maxY{};
    float size{};
    float paddingX{}, paddingY{};
    float hudPadding{};
    float dotRadius{};
    float radarHudScaling{};
};

RadarLayout compute_radar_layout(
    const CvarInputs& cvars,
    int displayW, int displayH,
    const HudRadarSnapshot& hud) noexcept;

// ====================================================================
// Dot visual helpers
// ====================================================================

/// Compute dot radius based on radar scale and icon min.
float polar_dot_radius(float radarScale, float iconScaleMin,
                        float hudScaling, float screenH) noexcept;

/// Stable health display (hold through bad frames).
int stable_display_health(int currentHealth, int previousHealth,
                           int& holdCounter) noexcept;

// ====================================================================
// Transform for display aspect ratio correction
// ====================================================================
struct HudTransform {
    float logicalW{};
    float logicalH{};
    float scaleX{1.0f};
    float scaleY{1.0f};
};

HudTransform get_hud_transform(int displayW, int displayH,
                                int gameW, int gameH) noexcept;

} // namespace real::cs2::periscope
