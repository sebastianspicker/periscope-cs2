// gui_widgets.cpp — Immediate-mode widget methods for GuiSystem.

#include "real/gpu/gui_internal.hpp"

namespace real::gpu::gui {

bool GuiSystem::button(const char* label, float w, float h) {
  Impl& i = *impl_;
  float bw = w > 0.f ? w : i.win.w - 2.f * i.style.pad;
  float bh = h > 0.f ? h : i.style.row_h;
  Rect r{i.win.x + i.style.pad, i.cursor_y - bh, bw, bh};
  i.cursor_y -= bh + i.style.pad;
  int id = i.next_id++;
  bool hover = i.mouse_in(r);
  bool pressed = hover && i.in.mouse_down;
  bool activated = i.click_on(id, r);
  if (i.rp) {
    uint32_t col = pressed ? i.style.button_active
                  : hover ? i.style.button_hover : i.style.button;
    i.rp->draw_rect_filled(r.x, r.y, r.w, r.h, col);
    i.rp->draw_rect(r.x, r.y, r.w, r.h, i.style.frame);
    i.draw_text(r, label, i.style.text);
  }
  return activated;
}

bool GuiSystem::checkbox(const char* label, bool* value) {
  Impl& i = *impl_;
  if (!value) return false;
  Rect r{i.win.x + i.style.pad, i.cursor_y - i.style.row_h, i.win.w - 2.f * i.style.pad,
         i.style.row_h};
  i.cursor_y -= i.style.row_h + i.style.pad;
  int id = i.next_id++;
  const bool toggled = i.click_on(id, r);
  if (toggled) {
    *value = !*value;
  }
  if (i.rp) {
    Rect box{r.x, r.y, i.style.row_h * 0.7f, i.style.row_h * 0.7f};
    uint32_t col = *value ? i.style.fill : i.style.track;
    i.rp->draw_rect_filled(box.x, box.y, box.w, box.h, col);
    i.rp->draw_rect(box.x, box.y, box.w, box.h, i.style.frame);
    if (*value) {
      i.rp->draw_line(box.x + box.w * 0.2f, box.y - box.h * 0.5f,
                      box.x + box.w * 0.5f, box.y - box.h * 0.75f, i.style.text);
      i.rp->draw_line(box.x + box.w * 0.5f, box.y - box.h * 0.75f,
                      box.x + box.w * 0.8f, box.y - box.h * 0.2f, i.style.text);
    }
    i.draw_text(Rect{box.x + box.w + i.style.pad, box.y, r.w - box.w - i.style.pad, box.h},
                label, i.style.text, false);
  }
  return toggled;
}

bool GuiSystem::slider_float(const char* label, float* value, float vmin,
                             float vmax, const char* fmt) {
  Impl& i = *impl_;
  if (!value) return false;
  Rect r{i.win.x + i.style.pad, i.cursor_y - i.style.row_h, i.win.w - 2.f * i.style.pad,
         i.style.row_h};
  i.cursor_y -= i.style.row_h + i.style.pad;
  int id = i.next_id++;
  bool changed = false;

  float track_h = i.style.row_h * 0.3f;
  Rect track{r.x, r.y - track_h * 0.5f, r.w, track_h};
  float span = (vmax - vmin) > 1e-6f ? (vmax - vmin) : 1.f;
  float t = (*value - vmin) / span;
  t = std::max(0.f, std::min(1.f, t));
  Rect fill{track.x, track.y, track.w * t, track.h};

  // Input is already window-relative pixels (y-down); track is normalized y-up.
  bool hover = i.mouse_in(track);
  if (hover) i.hover_id = id;
  if (hover && i.in.mouse_down) i.active_id = id;
  if (!i.in.mouse_down && i.active_id == id) {
    i.active_id = 0;
    i.any = true;
    changed = true;
  }
  if (i.active_id == id && i.in.mouse_down) {
    const float mx = i.in.mouse_x;
    const float tx = nx_to_px(track.x, i.win_w);
    const float tw = nx_to_px(track.x + track.w, i.win_w) - tx;
    if (tw > 1.f) {
      float nt = (mx - tx) / tw;
      nt = std::max(0.f, std::min(1.f, nt));
      *value = vmin + nt * span;
      i.any = true;
      changed = true;
    }
  }

  // Recompute fill after possible value mutation for accurate draw this frame.
  t = (*value - vmin) / span;
  t = std::max(0.f, std::min(1.f, t));
  fill.w = track.w * t;

  if (i.rp) {
    i.rp->draw_rect_filled(track.x, track.y, track.w, track.h, i.style.track);
    i.rp->draw_rect_filled(fill.x, fill.y, fill.w, fill.h, i.style.fill);
    float knob_size = track_h * 1.4f;
    i.rp->draw_rect_filled(track.x + track.w * t - knob_size * 0.5f,
                           track.y - knob_size * 0.4f, knob_size, knob_size,
                           i.style.button_hover);
    i.rp->draw_rect(track.x, track.y, track.w, track.h, i.style.frame);

    char value_buf[48];
    const char* use_fmt = fmt ? fmt : "%.2f";
    std::snprintf(value_buf, sizeof(value_buf), use_fmt, static_cast<double>(*value));
    i.rp->draw_text(r.x + r.w - 0.14f, r.y - r.h * 0.55f, value_buf,
                    i.style.text, i.style.text_size * 0.8f);
    i.rp->draw_text(r.x, r.y - r.h * 0.55f, label ? label : "", i.style.text_dim,
                    i.style.text_size * 0.8f);
  }
  return changed;
}

bool GuiSystem::slider_int(const char* label, int* value, int vmin, int vmax) {
  if (!value) return false;
  float f = static_cast<float>(*value);
  bool changed = slider_float(label, &f, static_cast<float>(vmin),
                              static_cast<float>(vmax), "%.0f");
  if (changed) {
    *value = static_cast<int>(std::lround(static_cast<double>(f)));
    if (*value < vmin) *value = vmin;
    if (*value > vmax) *value = vmax;
  }
  return changed;
}

bool GuiSystem::color_picker(const char* label, uint32_t* color) {
  Impl& i = *impl_;
  Rect r{i.win.x + i.style.pad, i.cursor_y - i.style.row_h, i.win.w - 2.f * i.style.pad,
         i.style.row_h};
  i.cursor_y -= i.style.row_h + i.style.pad;
  int id = i.next_id++;
  bool changed = false;

  if (i.rp) {
    Rect sw{r.x, r.y, i.style.row_h * 0.7f, i.style.row_h * 0.7f};
    i.rp->draw_rect_filled(sw.x, sw.y, sw.w, sw.h, *color);
    i.rp->draw_rect(sw.x, sw.y, sw.w, sw.h, i.style.frame);
    i.draw_text(Rect{sw.x + sw.w + i.style.pad, sw.y, r.w - sw.w - i.style.pad, r.h},
                label, i.style.text, false);
  }

  bool open_clicked = i.click_on(id, r);
  if (open_clicked) {
    i.picker_open = !i.picker_open;
    i.picker_id = id;
    rgb_to_hsv(*color, i.picker_h, i.picker_s, i.picker_v);
    if (i.picker_open) i.any = true;
  }

  if (i.picker_open && i.picker_id == id && i.rp) {
    Rect panel{r.x, r.y - 0.26f, r.w, 0.26f};
    i.rp->draw_rect_filled(panel.x, panel.y, panel.w, panel.h, i.style.panel);
    i.rp->draw_rect(panel.x, panel.y, panel.w, panel.h, i.style.frame);

    // Saturation/value square (x = saturation, y = value).
    Rect sv{panel.x + 0.008f, panel.y + 0.02f, 0.14f, 0.14f};
    const int grid = 8;
    for (int gy = 0; gy < grid; ++gy) {
      for (int gx = 0; gx < grid; ++gx) {
        float ss = gx / static_cast<float>(grid - 1);
        float vv = 1.f - gy / static_cast<float>(grid - 1);
        uint32_t cell = hsv_to_rgb(i.picker_h, ss, vv);
        Rect cr{sv.x + sv.w * gx / grid, sv.y + sv.h * (gy + 1) / grid,
                sv.w / grid, sv.h / grid};
        i.rp->draw_rect_filled(cr.x, cr.y, cr.w, cr.h, cell);
      }
    }
    int sv_id = i.next_id++;
    if (i.click_on(sv_id, sv) && i.in.mouse_down) {
      // mouse_x/y are already window pixels (y-down).
      const float sx = i.in.mouse_x;
      const float sy = i.in.mouse_y;
      const float px0 = nx_to_px(sv.x, i.win_w);
      const float py0 = ny_to_px(sv.y, i.win_h);
      const float pw = nx_to_px(sv.x + sv.w, i.win_w) - px0;
      const float ph = ny_to_px(sv.y - sv.h, i.win_h) - py0;
      if (pw > 1.f && ph > 1.f) {
        i.picker_s = std::max(0.f, std::min(1.f, (sx - px0) / pw));
        i.picker_v = std::max(0.f, std::min(1.f, (py0 + ph - sy) / ph));
      }
    }

    // Hue strip.
    Rect hue{panel.x + 0.16f, panel.y + 0.02f, 0.012f, 0.14f};
    for (int gy = 0; gy < grid; ++gy) {
      float hh = 360.f * (1.f - gy / static_cast<float>(grid - 1));
      Rect cr{hue.x, hue.y + hue.h * (gy + 1) / grid, hue.w, hue.h / grid};
      i.rp->draw_rect_filled(cr.x, cr.y, cr.w, cr.h, hsv_to_rgb(hh, 1.f, 1.f));
    }
    int hue_id = i.next_id++;
    if (i.click_on(hue_id, hue) && i.in.mouse_down) {
      const float sy = i.in.mouse_y;
      const float py0 = ny_to_px(hue.y, i.win_h);
      const float ph = ny_to_px(hue.y - hue.h, i.win_h) - py0;
      if (ph > 1.f) {
        i.picker_h = 360.f * (1.f - std::max(0.f, std::min(1.f, (py0 + ph - sy) / ph)));
      }
    }

    // Presets.
    const uint32_t presets[] = {
        0xFF444444, 0xFFCC0000, 0xFFCC8800, 0xFFCCCC00,
        0xFF00CC00, 0xFF00CCCC, 0xFF0000CC, 0xFFCC00CC,
        0xFFFFFFFF, 0xFF888888, 0xFF660000, 0xFF663300,
        0xFF333300, 0xFF003300, 0xFF003366, 0xFF330066};
    const int per_row = 8;
    float cell_w = 0.018f;
    for (int p = 0; p < 16; ++p) {
      int px = p % per_row;
      int py = p / per_row;
      Rect cr{panel.x + 0.16f + cell_w * 1.4f * px, panel.y + 0.17f - cell_w * py,
              cell_w, cell_w};
      int pid = i.next_id++;
      if (i.click_on(pid, cr)) {
        *color = presets[p];
        rgb_to_hsv(*color, i.picker_h, i.picker_s, i.picker_v);
        changed = true;
        i.picker_open = false;
        i.any = true;
      }
      i.rp->draw_rect_filled(cr.x, cr.y, cr.w, cr.h, presets[p]);
      i.rp->draw_rect(cr.x, cr.y, cr.w, cr.h, i.style.frame);
    }

    // Commit button.
    Rect ok{panel.x + panel.w - 0.09f, panel.y + 0.01f, 0.08f, 0.035f};
    int ok_id = i.next_id++;
    if (i.click_on(ok_id, ok)) {
      *color = hsv_to_rgb(i.picker_h, i.picker_s, i.picker_v);
      changed = true;
      i.picker_open = false;
      i.any = true;
    }
    i.rp->draw_rect_filled(ok.x, ok.y, ok.w, ok.h, i.style.accent);
    i.draw_text(ok, "OK", i.style.text, true);
  }

  return changed;
}

bool GuiSystem::selectable(const char* label, bool selected) {
  Impl& i = *impl_;
  Rect r{i.win.x + i.style.pad, i.cursor_y - i.style.row_h, i.win.w - 2.f * i.style.pad,
         i.style.row_h};
  i.cursor_y -= i.style.row_h + i.style.pad;
  int id = i.next_id++;
  if (i.rp) {
    uint32_t col = selected ? i.style.accent : i.style.button;
    i.rp->draw_rect_filled(r.x, r.y, r.w, r.h, col);
    i.rp->draw_rect(r.x, r.y, r.w, r.h, i.style.frame);
    i.draw_text(r, label, selected ? i.style.text : i.style.text_dim);
  }
  return i.click_on(id, r);
}

bool GuiSystem::text_input(const char* label, std::string* value) {
  Impl& i = *impl_;
  Rect r{i.win.x + i.style.pad, i.cursor_y - i.style.row_h, i.win.w - 2.f * i.style.pad,
         i.style.row_h};
  i.cursor_y -= i.style.row_h + i.style.pad;
  int id = i.next_id++;

  bool active = (i.text_editing && i.focused_text == label);
  bool changed = false;

  if (active) {
    // Apply typed text.
    for (char c : i.in.text_input) {
      if (c >= 0x20 && c < 0x7F) (*value) += c;
    }
    if (i.in.key_pressed && i.in.key_code == VK_BACK && !value->empty()) {
      value->pop_back();
    }
    if (i.in.key_pressed && i.in.key_code == VK_RETURN) {
      i.text_editing = false;
      i.any = true;
      changed = true;
    }
    if (i.in.mouse_clicked && !i.mouse_in(r)) {
      i.text_editing = false;
      i.any = true;
      changed = true;
    }
  } else {
    int fid = i.next_id++;
    if (i.click_on(fid, r)) {
      i.text_editing = true;
      i.focused_text = label;
      i.any = true;
    }
  }

  if (i.rp) {
    i.rp->draw_rect_filled(r.x, r.y, r.w, r.h, i.style.track);
    uint32_t col = active ? i.style.accent : i.style.frame;
    i.rp->draw_rect(r.x, r.y, r.w, r.h, col);
    std::string shown = *value;
    if (active && shown.size() < 64) shown += "_";
    i.draw_text(r, shown.c_str(), i.style.text, false);
  }
  (void)id;
  return changed;
}

bool GuiSystem::hotkey(const char* label, int* key) {
  Impl& i = *impl_;
  Rect r{i.win.x + i.style.pad, i.cursor_y - i.style.row_h, i.win.w - 2.f * i.style.pad,
         i.style.row_h};
  i.cursor_y -= i.style.row_h + i.style.pad;
  int id = i.next_id++;
  bool capturing = (i.hotkey_capture == label);
  bool changed = false;

  if (i.rp) {
    uint32_t col = capturing ? i.style.accent
                  : i.mouse_in(r) ? i.style.button_hover : i.style.button;
    i.rp->draw_rect_filled(r.x, r.y, r.w, r.h, col);
    i.rp->draw_rect(r.x, r.y, r.w, r.h, i.style.frame);
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s: %s%s", label, key_name(*key),
                  capturing ? " ..." : "");
    i.draw_text(r, buf, i.style.text, false);
  }

  if (capturing && i.in.key_pressed && i.in.key_code != 0) {
    *key = i.in.key_code;
    i.hotkey_capture.clear();
    i.any = true;
    return true;
  }
  if (i.click_on(id, r)) {
    i.hotkey_capture = label;
    i.any = true;
  }
  return changed;
}

void GuiSystem::label(const char* text) {
  Impl& i = *impl_;
  Rect r{i.win.x + i.style.pad, i.cursor_y - i.style.row_h, i.win.w - 2.f * i.style.pad,
         i.style.row_h};
  i.cursor_y -= i.style.row_h + i.style.pad;
  if (i.rp) i.draw_text(r, text, i.style.text, false);
}

void GuiSystem::labelf(const char* fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  label(buf);
}

void GuiSystem::separator() {
  Impl& i = *impl_;
  if (i.rp) {
    float y = i.cursor_y - i.style.row_h * 0.35f;
    i.rp->draw_line(i.win.x + i.style.pad, y, i.win.x + i.win.w - i.style.pad, y,
                    i.style.frame);
  }
  i.cursor_y -= i.style.row_h * 0.5f;
}

void GuiSystem::tooltip(const char* text) {
  impl_->tooltip_text = text;
}

bool GuiSystem::mouse_hovering() const {
  const Impl& i = *impl_;
  return i.in_window && i.mouse_in(i.win);
}

bool GuiSystem::any_activated() const { return impl_->any; }

}  // namespace real::gpu::gui
