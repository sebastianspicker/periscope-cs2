#pragma once

// External radar UI surface (window / web feed) — no game inject.
// Can register an OverlayWindow scar on sim::World for blue heuristics.
//
// Enhanced with StealthOverlay integration, WDA ensure every frame,
// randomized class/title from build seeds, and a real D3D11 blip render
// into a floating overlay window driven by present_overlay().

#include "ac/types.hpp"
#include "sim/world.hpp"

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
#include "real/gpu/periscope_overlay.hpp"
#include "real/gpu/render_pipeline.hpp"
#include "real/win/api_table.hpp"
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace t0_red {

/// Record a lab-only Band-4 overlay presentation owned by the cheat process.
void present_band4_overlay(sim::World& w, std::uint32_t explorer_pid,
                           std::uint32_t cheat_pid);

/// Record a lab-only DXGI desktop-duplication composite frame.
void present_dxgi_composite(sim::World& w);

// Lab type `RadarBlip` used by this educational unit.
struct RadarBlip {
  float map_x = 0;
  float map_y = 0;
  float world_x = 0;
  float world_z = 0;
  std::uint8_t team = 0;
  std::uint32_t entity_id = 0;
  bool enemy = false;
  bool is_local = false;
  float yaw = 0;  // eye_angles.y (degrees), for the facing arrow
};

// Lab type `RadarFrame` used by this educational unit.
struct RadarFrame {
  std::string title;
  int blip_count = 0;
  int enemy_count = 0;
  ac::Vec3 local{};
  std::string ascii_summary;  // CLI-friendly dump
};

// Lab type `RadarUi` used by this educational unit.
class RadarUi {
 public:
  ~RadarUi();

  void set_title(std::string title);
  void set_local_team(std::uint8_t t) { local_team_ = t; }
  void set_enemies_only(bool v) { enemies_only_ = v; }
  /// Set local player yaw (degrees) used to rotate the radar projection.
  void set_yaw_deg(float yaw);

  /// Project entities relative to local origin into blips.
  void update(const std::vector<ac::EntitySnapshot>& entities, ac::Vec3 local);

  /// Register external always-on-top window scar (does not touch game modules).
  void present_external_window(sim::World& w, std::uint32_t owner_pid);

  /// Register a simulated foreign-HWND presentation scar for blue PID checks.
  void present_hijacked_overlay(sim::World& w, std::uint32_t owner_pid,
                                std::uint32_t hwnd_owner_pid);

  /// Build a frame snapshot for demos/tests.
  RadarFrame frame() const;

  const std::vector<RadarBlip>& blips() const { return blips_; }
  const std::string& title() const { return title_; }
  int present_count() const { return present_count_; }

  // ── Stealth Overlay Integration ───────────────────────────────

  /// Initialize stealth overlay with randomized class/title from build seed.
  bool init_stealth_overlay(uint64_t class_seed, uint64_t surface_seed);

  /// Reapply WDA_EXCLUDEFROMCAPTURE. Must be called every frame.
  bool ensure_wda_every_frame();

  /// Present the stealth overlay.
  bool present_overlay();

  /// Process window messages (returns false on WM_QUIT/ESC).
  bool process_overlay_messages();

  /// Check if stealth overlay is active.
  bool has_stealth_overlay() const {
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
    return overlay_initialized_;
#else
    return false;
#endif
  }

  /// Tear down the render pipeline and the stealth overlay window.
  void shutdown_overlay();

 private:
  std::string title_ = "lab-radar";
  std::vector<RadarBlip> blips_;
  ac::Vec3 local_{};
  std::uint8_t local_team_ = 0;
  bool enemies_only_ = false;
  int present_count_ = 0;
  float yaw_deg_ = 0;

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
  real::gpu::periscope::StealthOverlay stealth_overlay_;
  bool overlay_initialized_ = false;
  int wda_reapply_counter_ = 0;
  real::gpu::RenderPipeline* pipeline_ = nullptr;
#endif
};

}  // namespace t0_red
