// gui_demo.cpp — Self-contained GUI radar demo (no CS2 required).
// Educational anti-cheat lab: simulated entities + immediate-mode control panel.
//
// Build: cmake -S . -B build
// Run:   ./build/gui_demo
// Keys:  INSERT=toggle control panel  END/ESC=quit
//
// Features: radar blips from make_sim_entities(), live RadarPanelSettings
// toggles (filter, ESP, health rings, crosshair), config persistence via
// load/save_radar_config, FPS readout.

#include "real/gpu/gui.hpp"
#include "real/gpu/render_pipeline.hpp"
#include "demos/radar_shared.hpp"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/timing.hpp"
#endif

#if defined(_WIN32)
// Console tool; force the console subsystem so the CRT uses main().
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRadarRadius = 0.88f;

// Build a RadarFrame from simulated entities, honoring RadarPanelSettings.
real::gpu::RadarFrame build_sim_frame(const std::vector<PlayerEntity>& ents,
                                      const real::gpu::gui::RadarPanelSettings& s) {
  real::gpu::RadarFrame frame{};
  frame.local_yaw = 0.f;
  frame.radar_scale = radar_world_scale(s.radar_scale);

  const PlayerEntity* local = nullptr;
  for (const auto& e : ents) {
    if (e.is_local) { local = &e; break; }
  }
  const ac::Vec3 origin = local ? local->origin : ac::Vec3{};
  const int local_team = local ? static_cast<int>(local->team) : 2;
  frame.local_origin = origin;

  for (const auto& e : ents) {
    if (!e.alive) continue;
    if (e.is_local) {
      real::gpu::RadarBlipLayout b{};
      b.screen_x = 0.f;
      b.screen_y = 0.f;
      b.angle = -kPi * 0.5f;
      b.color = 0xFFFFFFFF;
      b.health = e.health;
      b.team = e.team;
      b.is_alive = true;
      b.is_local = true;
      b.is_visible = true;
      std::snprintf(b.label, sizeof(b.label), "YOU");
      frame.blips.push_back(b);
      continue;
    }

    // Filter: 1 = enemies only, 2 = team only (local always shown).
    if (s.filter == 1 && e.team == local_team) continue;
    if (s.filter == 2 && e.team != local_team) continue;

    float rx = 0.f, ry = 0.f;
    project_world_to_radar(e.origin.x - origin.x, e.origin.y - origin.y, 0.f,
                           frame.radar_scale, rx, ry);

    real::gpu::RadarBlipLayout b{};
    b.screen_x = rx;
    b.screen_y = ry;
    b.angle = 0.f;
    b.color = (e.team == 3) ? 0xFF4488CC : 0xFFCC6644;
    b.health = e.health;
    b.team = e.team;
    b.is_alive = true;
    b.is_visible = true;

    const char* side = (e.team == 3) ? "CT" : "T";
    if (s.esp_health) {
      std::snprintf(b.label, sizeof(b.label), "%s%u %dhp", side, e.id, e.health);
    } else {
      std::snprintf(b.label, sizeof(b.label), "%s%u", side, e.id);
    }
    if (s.esp_distance) {
      const float d = std::sqrt((e.origin.x - origin.x) * (e.origin.x - origin.x) +
                                (e.origin.y - origin.y) * (e.origin.y - origin.y));
      char tail[16];
      std::snprintf(tail, sizeof(tail), " %.0fm", d / 100.f);
      std::strncat(b.label, tail, sizeof(b.label) - std::strlen(b.label) - 1);
    }
    frame.blips.push_back(b);
  }
  return frame;
}

// Radar rendering honoring RadarPanelSettings (radar_shared-style helpers).
void draw_sim_radar(real::gpu::RenderPipeline* rp,
                    const real::gpu::RadarFrame& frame,
                    const real::gpu::gui::RadarPanelSettings& s) {
  const float usable = kRadarRadius * 0.92f;

  if (s.draw_bg || s.draw_border) {
    const std::uint32_t bg = s.draw_bg ? s.radar_bg : 0x00000000;
    const std::uint32_t border = s.draw_border ? s.radar_border : 0x00000000;
    rp->draw_radar_background(0.f, 0.f, kRadarRadius, bg, border);
  }

  for (const auto& b : frame.blips) {
    if (b.is_local) {
      rp->draw_player_arrow(0.f, 0.f, b.angle, kRadarRadius * 0.10f, 0xFFFFFFFF);
      continue;
    }

    float x = b.screen_x * usable;
    float y = b.screen_y * usable;
    const float dist = std::sqrt(x * x + y * y);
    if (dist > usable && dist > 0.f) {
      x = x / dist * usable;
      y = y / dist * usable;
    }

    // ESP box: small colored rect under the blip.
    if (s.esp_boxes) {
      rp->draw_rect_filled(x - 0.018f, y + 0.032f, 0.036f, 0.009f, b.color);
    }

    if (s.enemy_arrows) {
      rp->draw_player_arrow(x, y, b.angle, kRadarRadius * 0.055f, b.color);
    } else {
      rp->draw_player_dot(x, y, 0.032f, b.color);
    }

    if (s.health_rings) {
      rp->draw_health_ring(x, y, 0.045f, b.health, b.armor);
    } else {
      rp->draw_health_bar(x - 0.028f, y + 0.035f, 0.055f, 0.009f, b.health);
    }

    if (s.esp_names && b.label[0] != '\0') {
      rp->draw_text(x + 0.03f, y - 0.012f, b.label, b.color, 0.025f);
    }
  }

  if (s.draw_crosshair) {
    rp->draw_crosshair(0.f, 0.f, 0.045f, s.crosshair_color);
  }
}

}  // namespace

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);

  real::gpu::OverlayStyle style{};
  style.anchor = real::gpu::OverlayStyle::Anchor::ScreenCorner;
  style.corner = real::gpu::OverlayStyle::Corner::TopRight;
  style.width = 640;
  style.height = 480;
  style.draw_background = false;
  style.always_on_top = true;
  style.clickthrough = true;
  style.window_alpha = 255;

  auto created = real::gpu::create_render_pipeline(style, "Gui Demo");
  if (!created) {
    std::fprintf(stderr, "gui_demo: failed to create render pipeline\n");
    return 1;
  }
  real::gpu::RenderPipeline* renderer = *created;
  if (!renderer->is_initialized()) {
    std::fprintf(stderr, "gui_demo: render pipeline not initialized\n");
    renderer->shutdown();
    delete renderer;
    return 1;
  }

  real::gpu::gui::GuiSystem gui;
  gui.set_renderer(renderer);
  gui.set_open(true);

  real::gpu::gui::RadarPanelSettings settings{};
  {
    const std::string cfg = real::gpu::gui::default_config_path();
    (void)real::gpu::gui::load_radar_config(cfg.c_str(), settings);
  }

  std::printf("=== Gui Demo (simulated entities, no CS2) ===\n");
  std::printf("Keys: INSERT=toggle panel  END/ESC=quit\n\n");

  // FPS bookkeeping (frames per second over a rolling 1s window).
  auto fps_start = std::chrono::steady_clock::now();
  int frame_count = 0;
  float fps = 0.f;

  bool insert_was = false;
  bool end_was = false;
  bool esc_was = false;
  bool lbtn_was = false;
  bool quit = false;
  // Finite auto-run for lab/CI (override with LR_DEMO_FRAMES, 0 = interactive).
  const int max_frames = radar_demo_max_frames(/*live=*/false);
  int total_frames = 0;
  int last_ents = 0;

  std::printf("gui_demo: sim entities ready (auto-frames=%d)\n", max_frames);

  while (!quit) {
    if (!renderer->process_messages()) break;

    const auto ents = make_sim_entities(2);
    last_ents = static_cast<int>(ents.size());

#if LR_PLATFORM_WINDOWS
    const bool insert_down = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    const bool end_down = (GetAsyncKeyState(VK_END) & 0x8000) != 0;
    const bool esc_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    const bool lbtn_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    if (insert_down && !insert_was) gui.toggle_open();
    if ((end_down && !end_was) || (esc_down && !esc_was)) quit = true;
    insert_was = insert_down;
    end_was = end_down;
    esc_was = esc_down;

    // Mouse in window-relative pixels (y-down), for GUI hit-testing.
    real::gpu::gui::GuiInput in;
    POINT cursor{};
    GetCursorPos(&cursor);
    if (void* hwnd = renderer->native_handle()) {
      RECT win_rect{};
      if (GetWindowRect(static_cast<HWND>(hwnd), &win_rect)) {
        in.mouse_x = static_cast<float>(cursor.x - win_rect.left);
        in.mouse_y = static_cast<float>(cursor.y - win_rect.top);
      }
    }
    in.mouse_down = lbtn_down;
    in.mouse_clicked = lbtn_down && !lbtn_was;  // rising edge
    in.wheel = 0;
    lbtn_was = lbtn_down;
    gui.set_input(in);
#endif

    const auto& st = renderer->overlay_style();
    const int win_w = st.width > 0 ? st.width : style.width;
    const int win_h = st.height > 0 ? st.height : style.height;

    if (auto r = renderer->begin_frame(); !r) break;
    gui.begin_frame(win_w, win_h);

    // Radar drawn first so the control panel layers on top.
    const real::gpu::RadarFrame frame = build_sim_frame(ents, settings);
    draw_sim_radar(renderer, frame, settings);

    if (gui.is_open()) {
      (void)real::gpu::gui::radar_panel(gui, settings);
      if (settings.show_fps) {
        gui.labelf("FPS: %.1f", static_cast<double>(fps));
      }
    }

    gui.end_frame();
    if (auto r = renderer->end_frame(); !r) break;
    renderer->maintain_overlay();

    frame_count++;
    ++total_frames;
    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - fps_start).count();
    if (elapsed >= 1.0) {
      fps = static_cast<float>(frame_count) / static_cast<float>(elapsed);
      frame_count = 0;
      fps_start = now;
      std::printf("\r[gui_demo] FPS: %.1f | entities: %d | filter=%d   ",
                  fps, last_ents, settings.filter);
      std::fflush(stdout);
    }

    if (max_frames > 0 && total_frames >= max_frames) {
      quit = true;
    }

#if LR_PLATFORM_WINDOWS
    real::win::fuzzed_sleep(16, 10);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
  }

  std::printf("\ngui_demo: complete entities=%d frames=%d\n", last_ents, total_frames);

  // Persist settings and tear down cleanly.
  {
    const std::string cfg = real::gpu::gui::default_config_path();
    (void)real::gpu::gui::save_radar_config(cfg.c_str(), settings);
  }
  renderer->shutdown();
  delete renderer;
  std::printf("gui_demo: exited cleanly\n");
  return 0;
}
