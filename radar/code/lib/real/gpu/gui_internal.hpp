// gui_internal.hpp — Shared Impl and helpers for gui*.cpp TUs.
#pragma once

#include "real/gpu/gui.hpp"
#include "real/gpu/render_pipeline.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#else
#ifndef VK_F1
#define VK_F1 0x70
#define VK_F12 0x7B
#define VK_RETURN 0x0D
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_TAB 0x09
#define VK_BACK 0x08
#define VK_DELETE 0x2E
#define VK_INSERT 0x2D
#define VK_HOME 0x24
#define VK_END 0x23
#define VK_UP 0x26
#define VK_DOWN 0x28
#define VK_LEFT 0x25
#define VK_RIGHT 0x27
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_MENU 0x12
#endif
#endif

namespace real::gpu::gui {

// Convert a window-relative pixel coordinate to normalized -1..1.
inline float px_to_nx(float px, int w) {
  return w > 0 ? (px / static_cast<float>(w)) * 2.0f - 1.0f : 0.f;
}
inline float px_to_ny(float py, int h) {
  return h > 0 ? 1.0f - (py / static_cast<float>(h)) * 2.0f : 0.f;
}

// Convert normalized back to pixels (for hit-testing convenience).
inline float nx_to_px(float nx, int w) {
  return (nx + 1.0f) * 0.5f * static_cast<float>(w);
}
inline float ny_to_px(float ny, int h) {
  return (1.0f - ny) * 0.5f * static_cast<float>(h);
}

inline uint32_t lerp_color(uint32_t a, uint32_t b, float t) {
  if (t <= 0.f) return a;
  if (t >= 1.f) return b;
  auto comp = [&](int sh) {
    int ca = static_cast<int>((a >> sh) & 0xFF);
    int cb = static_cast<int>((b >> sh) & 0xFF);
    return static_cast<uint32_t>(ca + static_cast<int>((cb - ca) * t));
  };
  return (comp(24) << 24) | (comp(16) << 16) | (comp(8) << 8) | comp(0);
}

inline void rgb_to_hsv(uint32_t color, float& h, float& s, float& v) {
  float r = ((color >> 16) & 0xFF) / 255.f;
  float g = ((color >> 8) & 0xFF) / 255.f;
  float b = (color & 0xFF) / 255.f;
  float mx = std::max(r, std::max(g, b));
  float mn = std::min(r, std::min(g, b));
  float d = mx - mn;
  v = mx;
  if (d < 1e-6f) { h = 0.f; s = 0.f; return; }
  s = d / mx;
  if (mx == r) h = 60.f * std::fmod((g - b) / d, 6.f);
  else if (mx == g) h = 60.f * ((b - r) / d + 2.f);
  else h = 60.f * ((r - g) / d + 4.f);
  if (h < 0.f) h += 360.f;
}

inline uint32_t hsv_to_rgb(float h, float s, float v) {
  h = std::fmod(h, 360.f);
  if (h < 0.f) h += 360.f;
  float c = v * s;
  float x = c * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f));
  float m = v - c;
  float r = 0, g = 0, b = 0;
  if (h < 60.f) { r = c; g = x; }
  else if (h < 120.f) { r = x; g = c; }
  else if (h < 180.f) { g = c; b = x; }
  else if (h < 240.f) { g = x; b = c; }
  else if (h < 300.f) { r = x; b = c; }
  else { r = c; b = x; }
  uint32_t R = static_cast<uint32_t>((r + m) * 255.f + 0.5f);
  uint32_t G = static_cast<uint32_t>((g + m) * 255.f + 0.5f);
  uint32_t B = static_cast<uint32_t>((b + m) * 255.f + 0.5f);
  return 0xFF000000 | (R << 16) | (G << 8) | B;
}

inline const char* key_name(int vk) {
  if (vk == 0) return "None";
  if (vk >= 0x30 && vk <= 0x39) {
    static char buf[2] = {'0' + static_cast<char>(vk - 0x30), 0};
    return buf;
  }
  if (vk >= 0x41 && vk <= 0x5A) {
    static char buf[2] = {'A' + static_cast<char>(vk - 0x41), 0};
    return buf;
  }
  if (vk >= VK_F1 && vk <= VK_F12) {
    static char buf[8];
    std::snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1);
    return buf;
  }
  switch (vk) {
    case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Esc";
    case VK_SPACE: return "Space";
    case VK_TAB: return "Tab";
    case VK_BACK: return "Backspace";
    case VK_DELETE: return "Del";
    case VK_INSERT: return "Ins";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_MENU: return "Alt";
    default: {
      static char buf[16];
      std::snprintf(buf, sizeof(buf), "0x%02X", vk & 0xFF);
      return buf;
    }
  }
}

struct GuiSystem::Impl {
  GuiStyle style;
  RenderPipeline* rp = nullptr;
  GuiInput in{};
  int win_w = 0;
  int win_h = 0;
  bool open = true;

  Rect win{};
  float cursor_y = 0;
  bool in_window = false;
  bool window_closed = false;

  std::vector<std::string> tabs;
  int active_tab = -1;
  bool tab_bar_active = false;

  int hover_id = 0;
  int active_id = 0;
  int next_id = 1;
  bool dragging = false;
  Vec2 drag_off{};

  std::string tooltip_text;
  std::string focused_text;
  bool text_editing = false;
  std::string hotkey_capture;
  bool picker_open = false;
  int picker_id = 0;
  float picker_h = 0.f;
  float picker_s = 1.f;
  float picker_v = 1.f;

  bool any = false;

  bool mouse_in(const Rect& r) const {
    float mx = nx_to_px(r.x, win_w);
    float my = ny_to_px(r.y, win_h);
    float mw = nx_to_px(r.x + r.w, win_w) - mx;
    float mh = ny_to_px(r.y - r.h, win_h) - my;
    return in.mouse_x >= mx && in.mouse_x <= mx + mw &&
           in.mouse_y >= my && in.mouse_y <= my + mh;
  }

  bool click_on(int id, const Rect& r) {
    bool hover = mouse_in(r);
    if (hover) hover_id = id;
    if (!in.mouse_down && active_id == id) active_id = 0;
    if (hover && in.mouse_down) active_id = id;
    if (hover && in.mouse_clicked && active_id == id) {
      any = true;
      return true;
    }
    return false;
  }

  void draw_text(const Rect& r, const char* text, uint32_t color,
                 bool center_h = true) {
    draw_text_sz(r, text, color, center_h, style.text_size);
  }

  void draw_text_sz(const Rect& r, const char* text, uint32_t color,
                    bool center_h, float size) {
    if (!rp || !text) return;
    float ty = r.y - r.h * 0.5f + size * 0.35f;
    if (center_h) {
      float text_w = static_cast<float>(std::strlen(text)) * size * 0.55f;
      rp->draw_text(r.x + (r.w - text_w) * 0.5f, ty, text, color, size);
    } else {
      rp->draw_text(r.x, ty, text, color, size);
    }
  }
};

}  // namespace real::gpu::gui
