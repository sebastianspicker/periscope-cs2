// periscope_radar.cpp — Radar coordinate math implementation.

#include "real/cs2/periscope_radar.hpp"
#include "real/cs2/periscope_hud.hpp"
#include "real/cs2/periscope_entity.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace real::cs2::periscope {

Vector2 polar_map_player_to_radar(
    const HudRadarSnapshot& hud,
    const Vector3& pos,
    float yaw,
    float radarToTextureScale,
    bool* outOfBounds) noexcept
{
    // Compute difference from map texture position
    float dx = pos.x - hud.mapTexturePosition.x;
    float dy = hud.mapTexturePosition.y - pos.y;

    // Apply texture scaling and offset
    float px = dx * radarToTextureScale - hud.originTexturePositionDifference.x;
    float py = dy * radarToTextureScale - hud.originTexturePositionDifference.y;
    float distSq = px * px + py * py;

    float x = px;
    float y = py;

    if (hud.isRound) {
        float cosYaw = std::cos(yaw);
        float sinYaw = std::sin(yaw);

        if (distSq >= hud.maxVisibilitySquared) {
            // Clamp to radar edge
            float scale = std::sqrt(hud.maxVisibilitySquared / distSq);
            x = -px * scale * cosYaw + py * scale * sinYaw;
            y = -px * scale * sinYaw - py * scale * cosYaw;
            if (outOfBounds) *outOfBounds = true;
        } else {
            x = -px * cosYaw + py * sinYaw;
            y = -px * sinYaw - py * cosYaw;
            if (outOfBounds) *outOfBounds = false;
        }
    }

    return {x, y};
}

RadarLayout compute_radar_layout(
    const CvarInputs& cvars,
    int displayW, int displayH,
    const HudRadarSnapshot& hud) noexcept
{
    RadarLayout layout{};

    float hudScaling = std::max(cvars.hudScaling, 0.01f);
    float safezoneX = cvars.safezoneX;
    float safezoneY = cvars.safezoneY;
    float clHudRadarScale = std::max(cvars.clHudRadarScale, 0.01f);

    float screenW = static_cast<float>(displayW);
    float screenH = static_cast<float>(displayH);
    float resScale = screenH / 1080.0f;

    layout.hudPadding = hudScaling * 5.0f * resScale;
    layout.paddingX = std::lerp(screenW * 0.5f, 0.0f, safezoneX) + layout.hudPadding;
    layout.paddingY = std::lerp(screenH * 0.5f, 0.0f, safezoneY) + layout.hudPadding;

    layout.radarHudScaling = clHudRadarScale * hudScaling * resScale;
    layout.size = 290.0f * layout.radarHudScaling;

    layout.centerX = layout.paddingX + layout.size * 0.5f;
    layout.centerY = layout.paddingY + layout.size * 0.5f;
    layout.minX = layout.paddingX;
    layout.minY = layout.paddingY;
    layout.maxX = layout.paddingX + layout.size;
    layout.maxY = layout.paddingY + layout.size;

    float radarScale = hud.isRound
        ? hud.radarScale
        : (hud.visibilitySize / hud.visibilitySizeMax);

    float iconScaleMin = std::max(cvars.clRadarIconScaleMin, 0.1f);
    layout.dotRadius = std::clamp(radarScale, 0.0f, 1.0f) *
        (1.25f - iconScaleMin) + iconScaleMin;
    layout.dotRadius *= 7.5f * layout.radarHudScaling * resScale;

    return layout;
}

float polar_dot_radius(float radarScale, float iconScaleMin,
                        float hudScaling, float screenH) noexcept
{
    float rs = std::clamp(radarScale, 0.0f, 1.0f);
    float ism = std::max(iconScaleMin, 0.1f);
    float resScale = screenH / 1080.0f;
    float rsh = std::max(hudScaling, 0.01f);
    float radHudScale = rsh * resScale;

    // Must match compute_radar_layout() formula exactly:
    // dotRadius = clamp(radarScale, 0, 1) * (1.25 - iconScaleMin) + iconScaleMin
    // dotRadius *= 7.5 * radHudScale
    float r = rs * (1.25f - ism) + ism;
    r *= 7.5f * radHudScale * resScale;
    return r;
}

int stable_display_health(int currentHealth, int previousHealth,
                           int& holdCounter) noexcept
{
    constexpr int kMaxHold = 3;
    if (currentHealth > 0 && currentHealth <= 100) {
        holdCounter = 0;
        return currentHealth;
    }
    if (holdCounter < kMaxHold && previousHealth > 0 && previousHealth <= 100) {
        ++holdCounter;
        return previousHealth;
    }
    return 0;
}

HudTransform get_hud_transform(int displayW, int displayH,
                                int gameW, int gameH) noexcept
{
    HudTransform transform{
        static_cast<float>(displayW),
        static_cast<float>(displayH),
        1.0f, 1.0f
    };

    if (displayW <= 0 || displayH <= 0 || gameW <= 0 || gameH <= 0) {
        return transform;
    }
    if (gameW < 640 || gameH < 480 || gameW > 7680 || gameH > 4320) {
        return transform;
    }

    float displayAspect = static_cast<float>(displayW) /
                          static_cast<float>(displayH);
    float gameAspect = static_cast<float>(gameW) /
                       static_cast<float>(gameH);

    if (std::abs(displayAspect - gameAspect) < 0.01f) {
        return transform;
    }

    transform.logicalW = static_cast<float>(gameW);
    transform.logicalH = static_cast<float>(gameH);
    transform.scaleX = static_cast<float>(displayW) / transform.logicalW;
    transform.scaleY = static_cast<float>(displayH) / transform.logicalH;

    return transform;
}

} // namespace real::cs2::periscope
