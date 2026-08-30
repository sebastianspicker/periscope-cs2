// gui.cpp — Immediate-mode GUI core (frame, window, tabs, style).
// Widget methods live in gui_widgets.cpp; radar panel in gui_radar_panel.cpp.

#include "real/gpu/gui_internal.hpp"

namespace real::gpu::gui {

// ====================================================================
// GuiSystem public methods (core)
// ====================================================================

GuiSystem::GuiSystem() : impl_(new Impl()) {}
GuiSystem::~GuiSystem() { delete impl_; }

void GuiSystem::set_style(const GuiStyle& s) { impl_->style = s; }
const GuiStyle& GuiSystem::style() const { return impl_->style; }

void GuiSystem::set_renderer(RenderPipeline* rp) { impl_->rp = rp; }
RenderPipeline* GuiSystem::renderer() const { return impl_->rp; }

void GuiSystem::set_input(const GuiInput& in) { impl_->in = in; }

void GuiSystem::begin_frame(int width_px, int height_px) {
  impl_->win_w = width_px;
  impl_->win_h = height_px;
  impl_->any = false;
  impl_->hover_id = 0;
  // Immediate-mode IDs must be stable across frames for drag/active state.
  impl_->next_id = 1;
  impl_->tooltip_text.clear();
  impl_->tab_bar_active = false;
  impl_->in_window = false;
  impl_->window_closed = false;
}

void GuiSystem::end_frame() {
  // Tooltip rendering (last widget hovered).
  if (impl_->rp && !impl_->tooltip_text.empty() && impl_->in_window) {
    Rect tip;
    tip.x = px_to_nx(impl_->in.mouse_x + 12.f, impl_->win_w);
    tip.y = px_to_ny(impl_->in.mouse_y + 16.f, impl_->win_h);
    tip.w = 0.24f;
    tip.h = 0.045f;
    if (tip.x + tip.w > 1.f) tip.x = 1.f - tip.w;
    impl_->rp->draw_rect_filled(tip.x, tip.y, tip.w, tip.h, impl_->style.panel);
    impl_->rp->draw_rect(tip.x, tip.y, tip.w, tip.h, impl_->style.frame);
    impl_->rp->draw_text(tip.x + 0.006f, tip.y - 0.028f, impl_->tooltip_text.c_str(),
                         impl_->style.text, impl_->style.text_size * 0.7f);
  }
}

bool GuiSystem::is_open() const { return impl_->open; }
void GuiSystem::set_open(bool open) { impl_->open = open; }
void GuiSystem::toggle_open() { impl_->open = !impl_->open; }

bool GuiSystem::begin_window(const char* title, float x, float y, float w,
                             float h, bool* open) {
  Impl& i = *impl_;
  i.in_window = true;
  i.window_closed = false;
  if (i.win.w == 0.f) {
    i.win = Rect{x, y, w, h};
  }
  i.cursor_y = i.win.y + i.win.h;

  if (!i.rp) return true;

  // Header bar.
  Rect head{i.win.x, i.win.y + i.win.h, i.win.w, 0.055f};
  i.rp->draw_rect_filled(head.x, head.y, head.w, head.h, i.style.header);
  i.rp->draw_rect(head.x, head.y, head.w, head.h, i.style.frame);
  i.draw_text(head, title, i.style.text);

  // Close button (X) at the right of the header.
  int close_id = i.next_id++;
  Rect close{head.x + head.w - 0.05f, head.y, 0.04f, head.h * 0.8f};
  if (i.click_on(close_id, close) && open) {
    *open = false;
    i.window_closed = true;
  }
  i.rp->draw_text(close.x, close.y - head.h * 0.4f, "X", i.style.text_dim,
                  i.style.text_size * 0.8f);

  // Body panel.
  i.rp->draw_rect_filled(i.win.x, i.win.y, i.win.w, i.win.h - head.h,
                         i.style.panel);
  i.rp->draw_rect(i.win.x, i.win.y, i.win.w, i.win.h - head.h, i.style.frame);

  // Dragging.
  int drag_id = i.next_id++;
  if (i.click_on(drag_id, head) && i.in.mouse_down) {
    i.dragging = true;
    i.drag_off.x = px_to_nx(i.in.mouse_x, i.win_w) - i.win.x;
    i.drag_off.y = px_to_ny(i.in.mouse_y, i.win_h) - (i.win.y + i.win.h);
  }
  if (i.dragging) {
    if (!i.in.mouse_down) i.dragging = false;
    else {
      i.win.x = px_to_nx(i.in.mouse_x, i.win_w) - i.drag_off.x;
      i.win.y = px_to_ny(i.in.mouse_y, i.win_h) - i.drag_off.y - i.win.h;
      i.win.x = std::max(-1.02f, std::min(1.02f - i.win.w, i.win.x));
      i.win.y = std::max(-1.02f, std::min(1.02f - i.win.h, i.win.y));
    }
  }

  return !i.window_closed;
}

void GuiSystem::end_window() {
  impl_->in_window = false;
}

void GuiSystem::begin_tab_bar() {
  Impl& i = *impl_;
  i.tab_bar_active = true;
  i.tabs.clear();
  if (i.active_tab < 0) i.active_tab = 0;
}

bool GuiSystem::begin_tab(const char* label) {
  Impl& i = *impl_;
  int index = static_cast<int>(i.tabs.size());
  i.tabs.push_back(label);
  if (!i.rp) return index == i.active_tab;

  // Draw the tab row: each tab is a button.
  float tab_w = i.win.w / 5.0f;
  float tab_h = 0.05f;
  float tab_y = i.win.y + i.win.h - 0.055f - tab_h;
  for (int t = 0; t < static_cast<int>(i.tabs.size()); ++t) {
    Rect tr{i.win.x + tab_w * t, tab_y, tab_w, tab_h};
    int tid = i.next_id++;
    bool active_tab_clicked = false;
    bool hover = i.mouse_in(tr);
    if (hover) i.hover_id = tid;
    if (hover && i.in.mouse_down) i.active_id = tid;
    if (hover && i.in.mouse_clicked && i.active_id == tid) {
      i.any = true;
      active_tab_clicked = true;
    }
    if (!i.in.mouse_down && i.active_id == tid) i.active_id = 0;
    uint32_t col = (t == i.active_tab) ? i.style.accent
                   : hover ? i.style.button_hover : i.style.button;
    i.rp->draw_rect_filled(tr.x, tr.y, tr.w, tr.h, col);
    i.rp->draw_rect(tr.x, tr.y, tr.w, tr.h, i.style.frame);
    i.draw_text_sz(tr, i.tabs[t].c_str(), i.style.text, true,
                   i.style.text_size * 0.8f);
    if (active_tab_clicked) i.active_tab = t;
  }
  // Body cursor below the tab row.
  i.cursor_y = tab_y - i.style.pad;
  return index == i.active_tab;
}

void GuiSystem::end_tab() {}

}  // namespace real::gpu::gui
