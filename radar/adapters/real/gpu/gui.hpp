// gui.hpp — Immediate-mode GUI widget framework for the real GPU backend.
//
// Self-contained header: all public state (style, input, settings) lives in
// plain structs defined here; the actual widget implementation is hidden
// behind a PIMPL (`struct Impl`) and lives in gui.cpp. This header therefore
// only declares the API — the .cpp implementer fills in the Impl.
//
// Coordinate conventions:
//   * All normalized GUI coordinates (widget rects, draw calls) use the
//     RenderPipeline space: -1..1, y-up, origin at screen center.
//   * Input is window-relative PIXELS with y-DOWN (mouse position from the
//     top-left corner of the overlay window, 0..width_px / 0..height_px).
//   * The Impl is responsible for converting input pixels to normalized y-up
//     coords and for snapping the mouse/cursor and clicks onto widget rects.
//
// Colors are 0xAABBGGRR (matches DrawVertex color layout).

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"
#include "real/gpu/render_pipeline.hpp"

#include <cstdarg>
#include <cstdint>
#include <string>
#include <vector>

namespace real::gpu::gui {

// ── Basic geometry ─────────────────────────────────────────────────
struct Vec2 { float x = 0, y = 0; };
struct Rect { float x = 0, y = 0, w = 0, h = 0; };

// Colors are 0xAABBGGRR (matches DrawVertex color layout).
struct GuiStyle {
  uint32_t bg             = 0xC0141418;
  uint32_t panel          = 0xD0202028;
  uint32_t header         = 0xFF2A2A34;
  uint32_t button         = 0xFF30303C;
  uint32_t button_hover   = 0xFF3A3A48;
  uint32_t button_active  = 0xFF2A2A34;
  uint32_t text           = 0xFFE8E8E8;
  uint32_t text_dim       = 0xFF909098;
  uint32_t accent         = 0xFF4488CC;
  uint32_t accent_hover   = 0xFF55A0E0;
  uint32_t track          = 0xFF1C1C24;
  uint32_t fill           = 0xFF4488CC;
  uint32_t frame          = 0xFF606068;
  float    rounding       = 4.f;
  float    text_size      = 0.048f;  // normalized glyph height
  float    row_h          = 0.062f;  // normalized row height
  float    pad            = 0.008f;  // normalized padding
};

// Input state for one frame. Mouse is window-relative, pixels, y-down.
struct GuiInput {
  float         mouse_x = 0;
  float         mouse_y = 0;
  bool          mouse_down = false;
  bool          mouse_clicked = false;  // rising edge
  float         wheel = 0;              // scroll delta, pixels
  bool          key_pressed = false;
  int           key_code = 0;           // VK_* virtual key
  bool          any_text = false;
  std::string   text_input;             // printable chars this frame
};

// Live radar control-panel settings (feature toggles a demo applies).
struct RadarPanelSettings {
  int     filter = 0;              // 0 all, 1 enemies, 2 team
  float   radar_scale = 0.7f;
  bool    draw_bg = false;
  bool    draw_border = false;
  bool    draw_crosshair = false;
  bool    enemy_arrows = true;
  bool    health_rings = true;
  uint32_t radar_bg = 0x00000000;
  uint32_t radar_border = 0x553A9ACC;
  uint32_t crosshair_color = 0x6644AACC;
  bool    esp_enabled = true;
  bool    esp_boxes = true;
  bool    esp_names = true;
  bool    esp_health = true;
  bool    esp_distance = false;
  bool    esp_visible_only = false;
  bool    aim_assist = false;
  float   aim_smooth = 6.f;
  float   aim_fov = 30.f;
  bool    triggerbot = false;
  float   trigger_delay_ms = 120.f;
  int     overlay_alpha = 255;
  bool    overlay_topmost = true;
  bool    overlay_clickthrough = true;
  bool    show_fps = true;
  bool    show_intel = true;
  bool    show_blue = true;
};

// ── Immediate-mode GUI system ──────────────────────────────────────
//
// Lifetime: construct, set_style (optional), set_renderer, then each frame:
//   set_input + begin_frame + widgets + end_frame.
// All widget calls must occur between begin_frame()/end_frame(). Widgets
// render immediately using the attached RenderPipeline and report activation
// for this frame only.
class GuiSystem {
public:
  GuiSystem();
  ~GuiSystem();
  GuiSystem(const GuiSystem&) = delete;
  GuiSystem& operator=(const GuiSystem&) = delete;

  // Replace the active style. Applies immediately to subsequent widgets.
  void set_style(const GuiStyle& s);
  // Current style. Defaults to a fresh GuiStyle{} until set_style is called.
  const GuiStyle& style() const;

  // Attach the pipeline used for all drawing. The caller owns the
  // RenderPipeline and must keep it alive for the life of the GuiSystem.
  // Pass nullptr to detach (widgets then skip drawing but still track state).
  void set_renderer(RenderPipeline* rp);
  // Currently attached pipeline (may be nullptr).
  RenderPipeline* renderer() const;

  // Install this frame's input. Should be called before begin_frame.
  void set_input(const GuiInput& in);

  // Begin/end a GUI frame. width_px/height_px = overlay window size in pixels.
  // Input pixels are converted to normalized y-up coordinates using these
  // dimensions. All widget calls must happen between begin/end.
  void begin_frame(int width_px, int height_px);
  void end_frame();

  // Whether the panel overlay is visible/enabled this frame.
  bool is_open() const;
  // Force visibility state.
  void set_open(bool open);
  // Flip visibility state.
  void toggle_open();

  // Draggable titled panel. Returns false when closed (or when close X pressed).
  // x/y/w/h are the initial rect in normalized -1..1 y-up coordinates; the
  // panel is draggable by its header and remains within the caller's space.
  // If `open` is non-null, a pressed close (X) button clears *open and returns
  // false; the caller typically guards the body with the return value.
  bool begin_window(const char* title, float x, float y, float w, float h,
                    bool* open = nullptr);
  // End the window opened by the most recent begin_window call.
  void end_window();

  // Begin a tab bar. All following begin_tab calls become tabs of this bar.
  // Must be called inside a window.
  void begin_tab_bar();
  // Begin a tab; returns true only while this tab is the active one, so the
  // caller wraps its tab content in `if (gui.begin_tab("Label")) { ... }`.
  // Tab state persists across frames (stored in the Impl).
  bool begin_tab(const char* label);  // true while this tab is active
  // End the active tab body.
  void end_tab();

  // Button with an optional explicit size (normalized). If w/h are 0 the
  // button sizes to the label. Returns true on a rising mouse click released
  // over it this frame. Hover uses style.button_hover, pressed button_active.
  bool button(const char* label, float w = 0.f, float h = 0.f);
  // Checkbox toggle bound to *value. Returns true when the user toggled it
  // this frame (caller then reads *value). Draws style.frame square + check.
  bool checkbox(const char* label, bool* value);
  // Horizontal drag slider. Returns true while the user is actively dragging
  // (value updates continuously). fmt formats the current value label.
  bool slider_float(const char* label, float* value, float vmin, float vmax,
                    const char* fmt = "%.2f");
  // Integer drag slider (step = 1). Returns true while dragging.
  bool slider_int(const char* label, int* value, int vmin, int vmax);
  // Small color swatch that opens a color edit popover on click. *color is
  // 0xAABBGGRR. Returns true when the user commits a change this frame.
  bool color_picker(const char* label, uint32_t* color);
  // Single-line selectable entry (used in lists/menus). `selected` only
  // affects the highlight. Returns true when clicked this frame.
  bool selectable(const char* label, bool selected);
  // Single-line text field bound to *value. Clicking focuses it; typed text
  // from GuiInput::text_input is appended and Backspace/Delete applied while
  // focused. Returns true on Enter (commit) or when focus is lost.
  bool text_input(const char* label, std::string* value);
  // Hotkey capture control. Clicking it puts the control into "listening"
  // mode; the next key from GuiInput (key_pressed + key_code) is captured into
  // *key (VK_* codes; 0 to clear). Returns true when a key is captured.
  bool hotkey(const char* label, int* key);
  // Static text label (wraps automatically).
  void label(const char* text);
  // printf-style static text label.
  void labelf(const char* fmt, ...);
  // Horizontal separator line.
  void separator();
  // Tooltip shown on the next frame while the mouse hovers the last widget.
  void tooltip(const char* text);

  // True when the mouse currently hovers any widget (including panels).
  bool mouse_hovering() const;
  // True if any widget reported activation (click/change/commit) this frame.
  bool any_activated() const;   // true if any widget activated this frame

private:
  struct Impl;
  Impl* impl_ = nullptr;
};

// ── Radar control panel ────────────────────────────────────────────
//
// Renders a tabbed control panel (tabs: Radar, ESP, Aim, Overlay, Misc) into
// `gui` binding directly to `s`. The demo owns the GuiSystem lifecycle
// (set_renderer + begin/end frames) and calls this between begin_window (or
// after begin_frame). Returns true while the panel is open.
bool radar_panel(GuiSystem& gui, RadarPanelSettings& s);

// ── INI-style persistence ──────────────────────────────────────────
//
// Load/save RadarPanelSettings as simple "key = value" lines. Missing keys
// keep their current values on load. Returns false on I/O failure or malformed
// content. Path may be absolute or relative to the working directory.
bool load_radar_config(const char* path, RadarPanelSettings& s);
bool save_radar_config(const char* path, const RadarPanelSettings& s);
// Default config path under the app's config dir, e.g. "<exe_name>.cfg".
// `exe_name` defaults to "radar" when null/empty. Caller frees the string.
std::string default_config_path(const char* exe_name = "radar");

}  // namespace real::gpu::gui
