// radar_shared.hpp — Shared radar rendering and projection code for all tiers.

#pragma once
#include "real/real_fwd.hpp"
#include "real/cs2/periscope_radar.hpp"
#include "ac/types.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

// ── Player entity (tier-agnostic) ──────────────────────────────────
struct PlayerEntity {
    uint32_t id = 0;
    ac::Vec3 origin{};
    ac::Vec3 angles{};
    uint8_t team = 0;
    int health = 0;
    bool alive = false;
    bool is_local = false;
};

// ── Radar blip (screen-space) ──────────────────────────────────────
struct RadarBlip {
    float x, y;
    float angle;
    uint32_t color;
    int health;
    bool is_local;
};

// ── Tier documentation ────────────────────────────────────────────
struct TierDoc {
    const char* name;
    const char* technique;
    const char* scar;
    const char* detection;
    const char* mitigation;
    const char* real_code;
};

// ── World → Radar projection ──────────────────────────────────────
// CS2 / Source2 is Z-up: horizontal plane is (X, Y), Z is height.
// Yaw 0 looks along +X; yaw +90 looks along +Y.
// With cl_radar_rotate 1, local forward is "up" on the minimap.
//
//   forward = (cos(yaw), sin(yaw))
//   right   = (sin(yaw), -cos(yaw))
//   screen_x =  (right · delta) / scale     // +X = right of view
//   screen_y = -(forward · delta) / scale   // ahead → up (draw path flips Y)
// World-space radius of the radar *disc edge* as a function of cl_radar_scale.
//
// CS2 / Source2 model (always-centered rotating radar):
//   higher cl_radar_scale  → more zoomed in → smaller world radius on the disc
//   edge_radius = kWorldEdgeAtScale1 / cl_radar_scale
//
// Diagnosis (live): with a previous constant that produced edge≈2150 @ 0.7,
// blips sat at ~½ the in-game radius (e.g. 949u → r=0.44 instead of ~0.88).
// That means the true edge is ~1075 @ 0.7, i.e. kWorldEdgeAtScale1 ≈ 750:
//   750 / 0.7 ≈ 1071
//
// This is the inverse-scale law, not a post-hoc "×2 on blip coords" fudge.
static inline float radar_world_scale(float cl_radar_scale = 0.7f) {
    // Shared with periscope_radar::world_edge_radius_for_cl_radar_scale.
    return real::cs2::periscope::world_edge_radius_for_cl_radar_scale(cl_radar_scale);
}

static inline void project_world_to_radar(float dx, float dy, float yaw_deg,
                                          float scale, float& out_rx, float& out_ry)
{
    if (scale <= 0.0f) scale = radar_world_scale();
    const float yaw = yaw_deg * (3.14159265f / 180.0f);
    const float cf = cosf(yaw), sf = sinf(yaw);
    const float forward = dx * cf + dy * sf;
    const float right = dx * sf - dy * cf;
    out_rx = right / scale;
    out_ry = -forward / scale;
}

static inline std::vector<RadarBlip> project_to_radar(
    const std::vector<PlayerEntity>& ents,
    const ac::Vec3& local_origin,
    float local_yaw_deg,
    float cl_radar_scale = 0.7f)
{
    const float scale = radar_world_scale(cl_radar_scale);

    std::vector<RadarBlip> blips;
    for (const auto& e : ents) {
        if (!e.alive) continue;
        float dx = e.origin.x - local_origin.x;
        float dy = e.origin.y - local_origin.y;  // horizontal Y, not Z
        float rx = 0.f, ry = 0.f;
        project_world_to_radar(dx, dy, local_yaw_deg, scale, rx, ry);
        uint32_t color = e.is_local ? 0xFFFFFFFF :
                         (e.team == 2) ? 0xFFCC6644 :
                         (e.team == 3) ? 0xFF4488CC : 0xFF888888;
        blips.push_back({rx, ry, e.angles.y, color, e.health, e.is_local});
    }
    return blips;
}

// ── Console radar renderer ─────────────────────────────────────────
static inline void render_console(const TierDoc& doc,
    const std::vector<PlayerEntity>& ents,
    const std::vector<RadarBlip>& blips, double us)
{
#ifndef NDEBUG
    std::printf("\n╔══════════════════════════════════════════════╗\n");
    std::printf("║  %-46s║\n", doc.name);
    std::printf("╚══════════════════════════════════════════════╝\n");
    std::printf("  T: %s\n", doc.technique);
    std::printf("  S: %s\n", doc.scar);
    std::printf("  B: %s\n", doc.detection);
    std::printf("  M: %s\n", doc.mitigation);
#endif

    const int N = 21;
    char grid[N][N];
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) grid[y][x] = '.';

    float s = (N / 2) * 0.9f;
    for (const auto& b : blips) {
        int gx = (int)(b.x * s + N / 2);
        int gy = (int)(b.y * s + N / 2);
        if (gx >= 0 && gx < N && gy >= 0 && gy < N)
            grid[gy][gx] = b.is_local ? 'O' : (b.color == 0xFFCC6644 ? 'X' : '+');
    }

    std::printf("\n  %d players | %.0fus total | %.1fus/entity\n",
        (int)ents.size(), us, ents.size() ? us / ents.size() : 0);
    std::printf("\n    ");
    for (int x = 0; x < N; x++) std::printf("%c", 'A' + (x % 26));
    std::printf("\n");
    for (int y = 0; y < N; y++) {
        std::printf("  %02d ", y);
        for (int x = 0; x < N; x++) std::printf("%c", grid[y][x]);
        std::printf("\n");
    }

    for (size_t i = 0; i < blips.size(); i++) {
        const auto& b = blips[i];
        const char* t = b.is_local ? "YOU" :
            (b.color == 0xFFCC6644) ? "T " :
            (b.color == 0xFF4488CC) ? "CT" : " ?";
        std::printf("  [%zu] %-3s hp=%3d radar=(%+.2f,%+.2f) angle=%.0f\n",
                    i, t, b.health, (double)b.x, (double)b.y, (double)b.angle);
    }
}

// ── Floating overlay defaults (snap onto CS2 in-game minimap) ─────
static inline real::gpu::OverlayStyle default_radar_overlay_style() {
    real::gpu::OverlayStyle style{};
    style.anchor = real::gpu::OverlayStyle::Anchor::InGameRadar;
    style.always_on_top = true;
    style.clickthrough = true;
    // Full content opacity; pure black is color-keyed → no black box.
    style.window_alpha = 255;
    style.draw_background = false;
    style.draw_border = false;
    style.draw_crosshair = false;
    style.radar_bg = 0x00000000;
    style.radar_border = 0x00000000;
    style.crosshair = 0x00000000;
    style.text = 0x00000000;
    style.text_dim = 0x00000000;
    // HUD knobs (overwritten from live cvars when available)
    style.hud_scaling = 1.0f;
    style.cl_hud_radar_scale = 1.0f;
    style.cl_radar_scale = 0.7f;
    style.safezonex = 1.0f;
    style.safezoney = 1.0f;
    style.size_mul = 1.0f;
    style.center_nudge_px = 24;  // scaled by resolution_center_nudge_px(h)
    style.draw_enemy_arrows = true;
    style.draw_health_rings = true;
    // Fallback if CS2 window not found:
    style.width = 220;
    style.height = 220;
    style.corner = real::gpu::OverlayStyle::Corner::TopLeft;
    style.margin_px = 8;
    return style;
}

// ── D3D11 radar renderer (Windows only) ───────────────────────────
#if LR_PLATFORM_WINDOWS
static inline void render_d3d11(real::gpu::RenderPipeline* rp,
    const std::vector<RadarBlip>& blips, int count)
{
    (void)blips;
    real::gpu::RadarFrame frame;
    for (const auto& b : blips) {
        real::gpu::RadarBlipLayout l;
        l.screen_x = b.x; l.screen_y = b.y; l.angle = b.angle;
        l.color = b.color; l.health = b.health;
        l.is_local = b.is_local; l.is_visible = true;
        std::snprintf(l.label, sizeof(l.label), "%s", b.is_local ? "YOU" : "");
        frame.blips.push_back(l);
    }

    rp->maintain_overlay();
    rp->begin_frame();
    rp->draw_radar_frame(frame);
    char info[64];
    std::snprintf(info, sizeof(info), "%d players", count);
    const auto& st = rp->overlay_style();
    rp->draw_text(-0.92f, 0.90f, info, st.text, 0.05f);
    rp->end_frame();
}
#endif

// ── Generate simulated entities (no CS2 needed) ────────────────────
static inline std::vector<PlayerEntity> make_sim_entities(int team = 2) {
    auto e = [](uint32_t id, float x, float z, uint8_t t, int hp, bool local) {
        PlayerEntity p;
        p.id = id; p.origin = {x, 0, z}; p.team = t;
        p.health = hp; p.alive = true; p.is_local = local;
        return p;
    };
    return {
        e(1, 0, 0, (uint8_t)team, 100, true),
        e(2, 1350, 100, 2, 100, false),
        e(3, 1200, -300, 2, 75, false),
        e(4, -500, 800, 3, 100, false),
        e(5, -800, 600, 3, 80, false),
        e(6, -600, -400, 3, 15, false),
    };
}

// Max frames when running without live CS2 so lab/CI demos exit cleanly.
// Live mode still loops until the user closes the overlay (or END).
static inline int radar_demo_max_frames(bool live) {
    if (live) return 0;  // 0 => unlimited
    if (const char* env = std::getenv("LR_DEMO_FRAMES")) {
        int n = std::atoi(env);
        if (n > 0) return n;
    }
    return 45;  // ~0.7s at 16ms — enough for entities + tier text
}

static inline void print_tier_surface(const TierDoc& doc, int entities, int blips,
                                      bool live) {
    std::printf("  Technique: %s\n", doc.technique);
    std::printf("  Scar:      %s\n", doc.scar);
    std::printf("  Detection: %s\n", doc.detection);
    std::printf("  Mitigation:%s\n", doc.mitigation);
    std::printf("  Code:      %s\n", doc.real_code);
    std::printf("  Mode:      %s | entities=%d blips=%d\n",
                live ? "LIVE" : "SIMULATION", entities, blips);
}

// ── Full render for both platforms ────────────────────────────────
static inline void render_radar(real::gpu::RenderPipeline* rp,
    const std::vector<PlayerEntity>& ents,
    const std::vector<RadarBlip>& blips,
    const TierDoc& doc, double us)
{
    if (rp) {
#if LR_PLATFORM_WINDOWS
        render_d3d11(rp, blips, (int)ents.size());
#endif
    } else {
        render_console(doc, ents, blips, us);
    }
    (void)doc;
}
