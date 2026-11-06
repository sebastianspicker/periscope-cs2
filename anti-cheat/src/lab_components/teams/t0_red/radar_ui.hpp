#pragma once

// External radar UI surface (window / web feed) — no game inject.
// Can register an OverlayWindow scar on sim::World for blue heuristics.
//
// Platform rendering is composed by real adapters, not this reusable lab component.

#include "ac/types.hpp"
#include "sim/world.hpp"


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

 private:
  std::string title_ = "lab-radar";
  std::vector<RadarBlip> blips_;
  ac::Vec3 local_{};
  std::uint8_t local_team_ = 0;
  bool enemies_only_ = false;
  int present_count_ = 0;
  float yaw_deg_ = 0;
};

}  // namespace t0_red
