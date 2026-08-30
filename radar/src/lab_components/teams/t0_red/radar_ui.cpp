// radar_ui.cpp — trivial radar view model over EntitySnapshot list (T0 demo).
// Formats enemy positions for narrated labs; no overlay/GPU work.
// Platform rendering is deliberately composed outside this lab component.

#include "t0_red/radar_ui.hpp"

#define OBF(x) x

#include <sstream>

namespace t0_red {

namespace {

void reapply_wda_if_requested(sim::World& w) {
  if (w.wda_excluded_from_capture) {
    ++w.wda_reapply_count;
  }
}

}  // namespace

void present_band4_overlay(sim::World& w, std::uint32_t explorer_pid,
                           std::uint32_t cheat_pid) {
  sim::OverlayWindow overlay;
  overlay.owner_pid = cheat_pid;
  overlay.title = OBF("lab-radar-band4");
  overlay.topmost = true;
  overlay.transparent = true;
  overlay.band4_zorder = true;
  overlay.z_order = sim::ZOrderBand::Band4;
  overlay.explorer_injected_for_band4 = true;
  overlay.band4_explorer_pid = explorer_pid;
  w.add_overlay(std::move(overlay));
  w.present_path_has_overlay = true;
  reapply_wda_if_requested(w);
  w.note("t0 present_band4_overlay explorer=" + std::to_string(explorer_pid) +
         " cheat=" + std::to_string(cheat_pid));
}

void present_dxgi_composite(sim::World& w) {
  w.dxgi_composite_active = true;
  w.desktop_duplication_active = true;
  w.desktop_duplication = true;
  w.present_path_has_overlay = true;
  ++w.composite_frames_rendered;
  if (w.capture_sensor_active && !w.capture_sees_overlays) {
    w.capture_vs_present_mismatch = true;
  }
  reapply_wda_if_requested(w);
  w.note("t0 present_dxgi_composite frame=" +
         std::to_string(w.composite_frames_rendered));
}

void RadarUi::set_title(std::string title) { title_ = std::move(title); }

void RadarUi::set_yaw_deg(float yaw) { yaw_deg_ = yaw; }

// RadarUi::update: Replace radar entity list shown to the demo UI.
void RadarUi::update(const std::vector<ac::EntitySnapshot>& entities,
                     ac::Vec3 local) {
  local_ = local;
  blips_.clear();
  for (const auto& e : entities) {
    if (!e.alive) {
      continue;
    }
    const bool enemy = (local_team_ == 0) ? (e.team != 0 && e.team != 1)
                                          : (e.team != local_team_);
    // When local_team is 0, treat team 2 as enemy of team 1 style lab data.
    bool is_enemy = e.team != local_team_;
    if (local_team_ == 0) {
      is_enemy = e.team >= 2 || e.team == 1;  // show all non-local lab teams
    }
    if (enemies_only_ && !is_enemy) {
      continue;
    }
    RadarBlip b;
    b.map_x = e.origin.x - local.x;
    b.map_y = e.origin.z - local.z;
    b.world_x = e.origin.x;
    b.world_z = e.origin.z;
    b.team = e.team;
    b.entity_id = e.id;
    b.enemy = is_enemy;
    b.is_local = e.is_local_player;
    b.yaw = e.eye_angles.y;
    blips_.push_back(b);
    (void)enemy;
  }
}

// RadarUi::present_external_window: Demo flag: present as external overlay window title path.
void RadarUi::present_external_window(sim::World& w, std::uint32_t owner_pid) {
  w.add_overlay(sim::OverlayWindow{owner_pid, title_, /*topmost=*/true,
                                   /*transparent=*/true,
                                   /*hijacks_swapchain=*/false});
  ++present_count_;
  w.note("t0 RadarUi present_external_window title=" + title_ +
         " owner=" + std::to_string(owner_pid));
}

void RadarUi::present_hijacked_overlay(sim::World& w, std::uint32_t owner_pid,
                                       std::uint32_t hwnd_owner_pid) {
  w.add_overlay(sim::OverlayWindow{hwnd_owner_pid, title_ + "-hijacked",
                                   /*topmost=*/true, /*transparent=*/true,
                                   /*hijacks_swapchain=*/true});
  ++present_count_;
  w.note("t0 RadarUi present_hijacked_overlay presenter=" +
         std::to_string(owner_pid) + " hwnd_owner=" +
         std::to_string(hwnd_owner_pid));
}

// RadarUi::frame: Build one radar frame string from cached entities.
RadarFrame RadarUi::frame() const {
  RadarFrame f;
  f.title = title_;
  f.blip_count = static_cast<int>(blips_.size());
  f.local = local_;
  for (const auto& b : blips_) {
    if (b.enemy) {
      ++f.enemy_count;
    }
  }
  std::ostringstream oss;
  oss << "radar\"" << title_ << "\" blips=" << f.blip_count
      << " enemies=" << f.enemy_count;
  for (const auto& b : blips_) {
    oss << " | id=" << b.entity_id << " t=" << static_cast<int>(b.team)
        << " (" << b.world_x << "," << b.world_z << ")";
  }
  f.ascii_summary = oss.str();
  return f;
}

}  // namespace t0_red
