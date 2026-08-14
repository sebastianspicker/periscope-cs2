// radar_ui.cpp — trivial radar view model over EntitySnapshot list (T0 demo).
// Formats enemy positions for narrated labs; no overlay/GPU work.
// Enhanced with StealthOverlay integration, WDA per frame, randomized names,
// and a real D3D11 blip render via present_overlay().

#include "t0_red/radar_ui.hpp"

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
#include "real/cs2/periscope_radar.hpp"
#include "real/gpu/render_pipeline.hpp"
#include "real/win/xorstr.hpp"
#else
#define OBF(x) x
#endif

#include <cmath>
#include <cstdio>
#include <sstream>

namespace t0_red {

namespace {

void reapply_wda_if_requested(sim::World& w) {
  if (w.wda_excluded_from_capture) {
    ++w.wda_reapply_count;
  }
}

}  // namespace

#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
namespace {

constexpr float kRadarPi = 3.14159265f;

// World-space radar edge radius at the default cl_radar_scale (0.7).
// Shared math with demos/radar_shared.hpp / periscope_radar.hpp.
float radar_world_scale() {
  return real::cs2::periscope::world_edge_radius_for_cl_radar_scale(0.7f);
}

// Create a D3D11 render pipeline with its own floating overlay window
// (ScreenCorner, top-right). The pipeline owns the window and re-asserts
// its position/topmost via maintain_overlay() every frame.
real::gpu::RenderPipeline* create_radar_pipeline(const char* title) {
  real::gpu::OverlayStyle style;
  style.width = 360;
  style.height = 360;
  style.anchor = real::gpu::OverlayStyle::Anchor::ScreenCorner;
  style.corner = real::gpu::OverlayStyle::Corner::TopRight;
  style.margin_px = 18;
  style.always_on_top = true;
  style.clickthrough = true;
  style.window_alpha = 255;
  style.draw_background = true;
  style.draw_border = true;
  style.draw_crosshair = true;
  style.draw_enemy_arrows = true;
  style.draw_health_rings = true;
  style.radar_bg = 0x55202838u;
  style.radar_border = 0x553A9ACC;
  style.crosshair = 0x6644AACC;
  style.text = 0xE0FFFFFF;
  style.text_dim = 0x80FFFFFF;
  const auto result = real::gpu::create_render_pipeline(style, title);
  return result.ok ? *result : nullptr;
}

}  // namespace
#endif

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

// ── Stealth Overlay Integration ───────────────────────────────

bool RadarUi::init_stealth_overlay(uint64_t class_seed, uint64_t surface_seed) {
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
  if (overlay_initialized_) return true;

  real::gpu::periscope::OverlayConfig config;
  config.width = 360;
  config.height = 360;
  config.alpha = 220;
  config.topmost = true;
  config.clickthrough = true;

  // Generate randomized class and window names from build seeds
  config.className = real::gpu::periscope::generate_random_name(
      class_seed ^ build::kClassSeed, "OvrClass");
  config.windowName = real::gpu::periscope::generate_random_name(
      surface_seed ^ build::kClassSeed, "WinApp");

  if (!stealth_overlay_.initialize(config)) return false;

  // Initial WDA application
  ensure_wda_every_frame();

  // Create the D3D11 pipeline that actually renders the radar blips into a
  // floating overlay window (ScreenCorner, top-right). The pipeline owns its
  // window; present_overlay() drives it each frame. Failure is non-fatal —
  // present_overlay() falls back gracefully.
  if (!pipeline_) {
    pipeline_ = create_radar_pipeline(config.windowName.c_str());
  }

  overlay_initialized_ = true;
  return true;
#else
  (void)class_seed;
  (void)surface_seed;
  return false;
#endif
}

bool RadarUi::ensure_wda_every_frame() {
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
  if (!overlay_initialized_) return false;
  bool ok = stealth_overlay_.ensure_capture_exclusion();
  if (ok) ++wda_reapply_counter_;
  return ok;
#else
  return false;
#endif
}

bool RadarUi::present_overlay() {
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
  if (!overlay_initialized_) return false;
  ensure_wda_every_frame();
  ++present_count_;

  // Lazy pipeline creation (e.g. init ran on an older seed path).
  if (!pipeline_) {
    pipeline_ = create_radar_pipeline("lab-radar");
  }
  if (!pipeline_) {
    // Graceful fallback: keep WDA + present counter so demos never crash.
    return true;
  }
  if (!pipeline_->process_messages()) {
    shutdown_overlay();
    return false;
  }

  // ── Build one render frame from the cached radar blips ──────────
  real::gpu::RadarFrame frame;
  frame.local_yaw = yaw_deg_ * (kRadarPi / 180.0f);
  frame.local_origin = local_;
  frame.radar_scale = radar_world_scale();
  const float scale =
      frame.radar_scale > 0.0f ? frame.radar_scale : 750.0f / 0.7f;
  const float yaw = yaw_deg_ * (kRadarPi / 180.0f);
  const float cf = std::cos(yaw);
  const float sf = std::sin(yaw);

  // Local player: white orientation arrow pinned to the radar center.
  real::gpu::RadarBlipLayout local;
  local.screen_x = 0.0f;
  local.screen_y = 0.0f;
  local.angle = -kRadarPi * 0.5f;
  local.color = 0xFFFFFFFF;
  local.health = 100;
  local.is_local = true;
  local.is_visible = true;
  std::snprintf(local.label, sizeof(local.label), "YOU");
  frame.blips.push_back(local);

  for (const auto& b : blips_) {
    if (b.is_local) continue;  // already drawn at the center
    real::gpu::RadarBlipLayout l;
    // Project the world delta (map_x, map_y) through local yaw to -1..1.
    const float dx = b.map_x;
    const float dy = b.map_y;
    const float forward = dx * cf + dy * sf;
    const float right = dx * sf - dy * cf;
    l.screen_x = right / scale;
    l.screen_y = -forward / scale;
    l.angle = (b.yaw - yaw_deg_) * (kRadarPi / 180.0f);
    l.color = (b.team == 2) ? 0xFFCC6644 : (b.team == 3) ? 0xFF4488CC
                                                         : 0xFF888888;
    l.health = 100;
    l.team = b.team;
    l.is_alive = true;
    l.is_visible = true;
    frame.blips.push_back(l);
  }

  (void)pipeline_->begin_frame();
  (void)pipeline_->draw_radar_frame(frame);
  (void)pipeline_->end_frame();
  pipeline_->maintain_overlay();
  return true;
#else
  return false;
#endif
}

void RadarUi::shutdown_overlay() {
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
  if (pipeline_) {
    (void)pipeline_->shutdown();
    delete pipeline_;
    pipeline_ = nullptr;
  }
  stealth_overlay_.shutdown();
  overlay_initialized_ = false;
#else
  // No platform resources to release.
#endif
}

RadarUi::~RadarUi() { shutdown_overlay(); }

bool RadarUi::process_overlay_messages() {
#if LR_HAS_REAL_PLATFORM && LR_PLATFORM_WINDOWS
  if (!overlay_initialized_) return true;
  return stealth_overlay_.process_messages();
#else
  return true;
#endif
}

}  // namespace t0_red
