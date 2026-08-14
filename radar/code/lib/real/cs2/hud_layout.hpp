// hud_layout.hpp — Read CS2 HUD/radar cvars and compute minimap screen rect.
// Educational: shows how external tools derive overlay placement from live settings.
#pragma once

#include "real/cs2/memory.hpp"
#include "real/error.hpp"
#include "real/gpu/render_pipeline.hpp"

#include <cstdint>

namespace real::cs2 {

/// Live HUD / radar settings sampled from process memory (or defaults).
struct RadarHudSettings {
  float hud_scaling = 1.0f;          ///< hud_scaling
  float cl_hud_radar_scale = 1.0f;   ///< cl_hud_radar_scale (panel size)
  float cl_radar_scale = 0.7f;       ///< cl_radar_scale (map zoom — not panel size)
  float safezonex = 1.0f;
  float safezoney = 1.0f;
  bool cl_radar_rotate = true;
  bool valid = false;                ///< true if at least one cvar was resolved from memory
  int resolved_count = 0;
};

/// Scan client.dll (and optional modules) for known ConVar name strings and
/// probe nearby floats. Falls back to OverlayStyle defaults when not found.
RadarHudSettings read_radar_hud_settings(Cs2MemoryReader& reader,
                                         std::uint64_t client_base,
                                         std::size_t client_size);

/// Apply discovered settings onto an OverlayStyle (keeps size_mul / force_*).
void apply_hud_settings_to_style(const RadarHudSettings& hud,
                                 real::gpu::OverlayStyle& style);

/// Compute minimap screen rect using client window + HUD settings.
/// Prefer this over the bare style-only formula when settings are live.
real::gpu::InGameRadarLayout compute_radar_screen_rect(
    void* game_hwnd, const RadarHudSettings& hud,
    const real::gpu::OverlayStyle& style);

}  // namespace real::cs2
