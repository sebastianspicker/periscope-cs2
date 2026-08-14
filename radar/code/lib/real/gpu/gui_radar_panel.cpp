// gui_radar_panel.cpp — Radar control panel and INI config persistence.

#include "real/gpu/gui_internal.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace real::gpu::gui {

bool radar_panel(GuiSystem& gui, RadarPanelSettings& s) {
  if (!gui.is_open()) return false;
  bool any = false;
  bool open = true;
  if (!gui.begin_window("Radar Control", -0.98f, 0.97f, 0.62f, 1.92f, &open)) {
    // Close (X) pressed — hide the panel for good until INSERT re-opens it.
    gui.set_open(false);
    gui.end_window();
    return false;
  }

  gui.begin_tab_bar();

  if (gui.begin_tab("Radar")) {
    const char* filter_names[] = {"All", "Enemies", "Team"};
    if (gui.selectable(filter_names[0], s.filter == 0)) s.filter = 0;
    if (gui.selectable(filter_names[1], s.filter == 1)) s.filter = 1;
    if (gui.selectable(filter_names[2], s.filter == 2)) s.filter = 2;
    gui.separator();
    any |= gui.slider_float("Radar scale", &s.radar_scale, 0.2f, 1.5f, "%.2f");
    any |= gui.checkbox("Draw background", &s.draw_bg);
    any |= gui.checkbox("Draw border", &s.draw_border);
    any |= gui.checkbox("Draw crosshair", &s.draw_crosshair);
    any |= gui.checkbox("Enemy arrows", &s.enemy_arrows);
    any |= gui.checkbox("Health rings", &s.health_rings);
    gui.separator();
    any |= gui.color_picker("Radar BG", &s.radar_bg);
    any |= gui.color_picker("Radar border", &s.radar_border);
    any |= gui.color_picker("Crosshair", &s.crosshair_color);
  }
  gui.end_tab();

  if (gui.begin_tab("ESP")) {
    any |= gui.checkbox("ESP enabled", &s.esp_enabled);
    any |= gui.checkbox("Boxes", &s.esp_boxes);
    any |= gui.checkbox("Names", &s.esp_names);
    any |= gui.checkbox("Health", &s.esp_health);
    any |= gui.checkbox("Distance", &s.esp_distance);
    any |= gui.checkbox("Visible only", &s.esp_visible_only);
  }
  gui.end_tab();

  if (gui.begin_tab("Aim")) {
    any |= gui.checkbox("Aim assist", &s.aim_assist);
    any |= gui.slider_float("Smooth", &s.aim_smooth, 0.f, 20.f, "%.1f");
    any |= gui.slider_float("FOV", &s.aim_fov, 1.f, 120.f, "%.0f");
    gui.separator();
    any |= gui.checkbox("Triggerbot", &s.triggerbot);
    any |= gui.slider_float("Delay (ms)", &s.trigger_delay_ms, 0.f, 500.f, "%.0f");
  }
  gui.end_tab();

  if (gui.begin_tab("Overlay")) {
    any |= gui.slider_int("Alpha", &s.overlay_alpha, 0, 255);
    any |= gui.checkbox("Topmost", &s.overlay_topmost);
    any |= gui.checkbox("Click-through", &s.overlay_clickthrough);
  }
  gui.end_tab();

  if (gui.begin_tab("Misc")) {
    any |= gui.checkbox("Show FPS", &s.show_fps);
    any |= gui.checkbox("Show intel", &s.show_intel);
    any |= gui.checkbox("Show blue score", &s.show_blue);
    gui.separator();
    if (gui.button("Save Config")) {
      save_radar_config(default_config_path().c_str(), s);
      any = true;
    }
    if (gui.button("Load Config")) {
      load_radar_config(default_config_path().c_str(), s);
      any = true;
    }
  }
  gui.end_tab();

  gui.end_window();
  any |= gui.any_activated();
  return any;
}

// ====================================================================
// Persistence
// ====================================================================

std::string default_config_path(const char* exe_name) {
#if LR_PLATFORM_WINDOWS
  char buf[MAX_PATH] = {};
  if (GetModuleFileNameA(nullptr, buf, MAX_PATH) > 0) {
    std::string path(buf);
    auto slash = path.find_last_of("\\/");
    if (slash != std::string::npos) {
      std::string base = exe_name && exe_name[0] ? exe_name : "radar";
      return path.substr(0, slash + 1) + base + ".ini";
    }
  }
#endif
  (void)exe_name;
  return "radar.ini";
}

static void trim(std::string& s) {
  auto not_space = [](char c) { return c != ' ' && c != '\t' && c != '\r'; };
  auto b = std::find_if(s.begin(), s.end(), not_space);
  auto e = std::find_if(s.rbegin(), s.rend(), not_space).base();
  s = (b < e) ? std::string(b, e) : std::string();
}

bool save_radar_config(const char* path, const RadarPanelSettings& s) {
  std::ofstream out(path, std::ios::trunc);
  if (!out) return false;
  auto wr = [&](const char* k, const std::string& v) { out << k << " = " << v << "\n"; };
  auto wrb = [&](const char* k, bool b) { wr(k, b ? "1" : "0"); };
  auto wri = [&](const char* k, int v) { wr(k, std::to_string(v)); };
  auto wrf = [&](const char* k, float v) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.6f", static_cast<double>(v));
    wr(k, buf);
  };
  auto wrc = [&](const char* k, uint32_t c) {
    char buf[24];
    std::snprintf(buf, sizeof(buf), "0x%08X", c);
    wr(k, buf);
  };
  wri("filter", s.filter);
  wrf("radar_scale", s.radar_scale);
  wrb("draw_bg", s.draw_bg);
  wrb("draw_border", s.draw_border);
  wrb("draw_crosshair", s.draw_crosshair);
  wrb("enemy_arrows", s.enemy_arrows);
  wrb("health_rings", s.health_rings);
  wrc("radar_bg", s.radar_bg);
  wrc("radar_border", s.radar_border);
  wrc("crosshair_color", s.crosshair_color);
  wrb("esp_enabled", s.esp_enabled);
  wrb("esp_boxes", s.esp_boxes);
  wrb("esp_names", s.esp_names);
  wrb("esp_health", s.esp_health);
  wrb("esp_distance", s.esp_distance);
  wrb("esp_visible_only", s.esp_visible_only);
  wrb("aim_assist", s.aim_assist);
  wrf("aim_smooth", s.aim_smooth);
  wrf("aim_fov", s.aim_fov);
  wrb("triggerbot", s.triggerbot);
  wrf("trigger_delay_ms", s.trigger_delay_ms);
  wri("overlay_alpha", s.overlay_alpha);
  wrb("overlay_topmost", s.overlay_topmost);
  wrb("overlay_clickthrough", s.overlay_clickthrough);
  wrb("show_fps", s.show_fps);
  wrb("show_intel", s.show_intel);
  wrb("show_blue", s.show_blue);
  return out.good();
}

bool load_radar_config(const char* path, RadarPanelSettings& s) {
  std::ifstream in(path);
  if (!in) return false;
  std::string line;
  while (std::getline(in, line)) {
    trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') continue;
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    trim(key);
    trim(val);
    auto setb = [&](bool& dst) { dst = (val == "1" || val == "true" || val == "yes"); };
    auto seti = [&](int& dst) { dst = static_cast<int>(std::strtol(val.c_str(), nullptr, 10)); };
    auto setf = [&](float& dst) { dst = std::strtof(val.c_str(), nullptr); };
    auto setc = [&](uint32_t& dst) {
      const char* p = val.c_str();
      if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
      dst = static_cast<uint32_t>(std::strtoul(p, nullptr, 16));
    };
    if (key == "filter") seti(s.filter);
    else if (key == "radar_scale") setf(s.radar_scale);
    else if (key == "draw_bg") setb(s.draw_bg);
    else if (key == "draw_border") setb(s.draw_border);
    else if (key == "draw_crosshair") setb(s.draw_crosshair);
    else if (key == "enemy_arrows") setb(s.enemy_arrows);
    else if (key == "health_rings") setb(s.health_rings);
    else if (key == "radar_bg") setc(s.radar_bg);
    else if (key == "radar_border") setc(s.radar_border);
    else if (key == "crosshair_color") setc(s.crosshair_color);
    else if (key == "esp_enabled") setb(s.esp_enabled);
    else if (key == "esp_boxes") setb(s.esp_boxes);
    else if (key == "esp_names") setb(s.esp_names);
    else if (key == "esp_health") setb(s.esp_health);
    else if (key == "esp_distance") setb(s.esp_distance);
    else if (key == "esp_visible_only") setb(s.esp_visible_only);
    else if (key == "aim_assist") setb(s.aim_assist);
    else if (key == "aim_smooth") setf(s.aim_smooth);
    else if (key == "aim_fov") setf(s.aim_fov);
    else if (key == "triggerbot") setb(s.triggerbot);
    else if (key == "trigger_delay_ms") setf(s.trigger_delay_ms);
    else if (key == "overlay_alpha") seti(s.overlay_alpha);
    else if (key == "overlay_topmost") setb(s.overlay_topmost);
    else if (key == "overlay_clickthrough") setb(s.overlay_clickthrough);
    else if (key == "show_fps") setb(s.show_fps);
    else if (key == "show_intel") setb(s.show_intel);
    else if (key == "show_blue") setb(s.show_blue);
  }
  return true;
}

}  // namespace real::gpu::gui
