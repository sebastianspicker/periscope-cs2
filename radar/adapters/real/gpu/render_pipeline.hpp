#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"
#include "ac/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::gpu {

enum class GpuApi;

// ── Drawing primitives ────────────────────────────────────────────

struct DrawVertex {
  float x, y;         // screen position (-1..1)
  float u, v;         // texture UV
  uint32_t color;     // 0xAABBGGRR
};

struct DrawCommand {
  enum Type { TriangleList, LineList, LineStrip };
  Type type = TriangleList;
  uint32_t vertex_offset = 0;
  uint32_t vertex_count = 0;
  uint32_t index_offset = 0;
  uint32_t index_count = 0;
};

// ── Player radar blip layout ──────────────────────────────────────
struct RadarBlipLayout {
  float screen_x, screen_y;  // -1..1 normalized
  float angle;               // orientation in radians
  uint32_t color;            // team color
  int health;                // 0-100
  int armor = 0;             // 0-100
  uint8_t team = 0;
  bool is_alive = false;
  bool is_local = false;
  bool is_visible = false;
  bool is_bomb = false;
  bool is_hostage = false;
  char label[32]{};
};

struct RadarFrame {
  std::vector<RadarBlipLayout> blips;
  float local_yaw;
  ac::Vec3 local_origin;
  float radar_scale;
};

/// Floating radar overlay appearance (size, alpha, anchor, z-order).
struct OverlayStyle {
  enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };
  /// Where the overlay is placed each frame.
  enum class Anchor {
    ScreenCorner,  ///< Fixed corner of the desktop work area
    InGameRadar,   ///< Snap on top of CS2's in-game minimap (default for demos)
  };

  int width = 320;                 ///< Window width (ScreenCorner, or fallback)
  int height = 320;                ///< Window height (ScreenCorner, or fallback)
  int margin_px = 18;              ///< Inset from work-area edges (ScreenCorner)
  Corner corner = Corner::TopRight;
  Anchor anchor = Anchor::InGameRadar;
  bool always_on_top = true;
  bool clickthrough = true;
  /// Blip / stroke opacity 0–255 applied via layered window (non-keyed pixels).
  /// Black pixels are color-keyed fully transparent (no black box).
  std::uint8_t window_alpha = 255;
  /// When true (default for InGameRadar), skip filled radar disc — only blips.
  bool draw_background = false;
  /// When true, draw a faint border ring (usually off for in-game snap).
  bool draw_border = false;
  /// When true, draw center crosshair.
  bool draw_crosshair = false;
  /// Debug: faint range ring/disc (for non-overlap / lab mode).
  bool debug_range_ring = false;
  /// Draw enemy facing arrows (not just local).
  bool draw_enemy_arrows = true;
  /// Draw health/armor rings around dots (no filled panel).
  bool draw_health_rings = true;
  /// Radar disc fill/border (0xAABBGGRR). Avoid pure black (color-keyed out).
  std::uint32_t radar_bg = 0x00000000;
  std::uint32_t radar_border = 0x553A9ACC;
  std::uint32_t crosshair = 0x6644AACC;
  std::uint32_t text = 0x00FFFFFF;  ///< default hidden on minimap snap
  std::uint32_t text_dim = 0x00FFFFFF;

  // ── In-game radar alignment (Source2 HUD, height-relative) ──────
  /// Matches CS2 `hud_scaling` (settings slider ~0.5–0.95+, default ~1).
  float hud_scaling = 1.0f;
  /// Matches CS2 `cl_hud_radar_scale` (0.8–1.3, default 1.0).
  float cl_hud_radar_scale = 1.0f;
  /// Matches CS2 `cl_radar_scale` (0.25–1.0, default 0.7) — map zoom, not panel size.
  float cl_radar_scale = 0.7f;
  /// Matches CS2 `safezonex` / `safezoney` (0–1, default 1 = flush edges).
  float safezonex = 1.0f;
  float safezoney = 1.0f;
  /// Fine-tune size multiplier after formula (1.0 = calibrated default).
  float size_mul = 1.0f;
  /// Extra pixel inset after safezone (positive = right / down). -1 = auto.
  int extra_inset_x = -1;
  int extra_inset_y = -1;
  /// Equal SE bias (px) so overlay center matches in-game map center.
  /// Positive moves overlay down AND right by the same amount. 24 = live-calibrated.
  int center_nudge_px = 24;
  /// Manual override (0 = auto size). Useful for LR_RADAR_SIZE env calibration.
  int force_size = 0;
  /// When true, force_x/force_y are absolute screen coords for the overlay top-left.
  bool force_pos = false;
  int force_x = 0;
  int force_y = 0;
};

/// Screen-space rect for the CS2 minimap (absolute screen coords).
struct InGameRadarLayout {
  int x = 0;
  int y = 0;
  int size = 0;
  int client_w = 0;
  int client_h = 0;
  void* game_hwnd = nullptr;
  bool valid = false;
  float used_hud_scaling = 1.0f;
  float used_cl_hud_radar_scale = 1.0f;
  float used_safezonex = 1.0f;
  float used_safezoney = 1.0f;
  int used_nudge = 0;       ///< equal SE bias applied (px)
  int used_edge_inset = 0;  ///< base corner pad (px)
};

// ── RadarBlip ↔ RadarBlipLayout bridge (was orphaned conversion) ──
/// Convert from game entity radar data to render-ready blip layout.
/// Priority: prefer real::cs2::RadarBlip entity coordinates transformed
/// through the HUD radar snapshot. Falls back to direct coordinate copy.
RadarBlipLayout blip_from_entity(float x, float y, float z, uint8_t team,
                                  bool alive, const char* label, uint32_t color,
                                  float yaw);

// ── Render Pipeline ───────────────────────────────────────────────

class RenderPipeline {
public:
  virtual ~RenderPipeline() = default;
  virtual GpuApi api() const noexcept = 0;
  virtual const char* name() const noexcept = 0;

  // Window management
  virtual Result<void> initialize(int width, int height, const char* title) = 0;
  virtual Result<void> shutdown() = 0;
  virtual bool is_initialized() const noexcept = 0;
  virtual void* native_handle() const noexcept = 0;  // HWND

  /// Apply size/alpha/corner/topmost. Safe to call after initialize.
  virtual Result<void> apply_overlay_style(const OverlayStyle& style) = 0;
  /// Re-assert HWND_TOPMOST + optional reposition (call each frame or periodically).
  virtual void maintain_overlay() = 0;
  virtual const OverlayStyle& overlay_style() const noexcept = 0;

  // Frame rendering
  virtual Result<void> begin_frame() = 0;
  virtual Result<void> end_frame() = 0;

  // ── Radar drawing helpers (built on primitives) ─────────────────
  virtual Result<void> draw_radar_background(float cx, float cy, float radius,
                                              uint32_t bg_color, uint32_t border_color) = 0;
  virtual Result<void> draw_player_dot(float x, float y, float radius,
                                       uint32_t color) = 0;
  virtual Result<void> draw_player_arrow(float x, float y, float angle,
                                         float size, uint32_t color) = 0;
  virtual Result<void> draw_health_bar(float x, float y, float width,
                                       float height, int health) = 0;
  /// Thin ring around a blip (health green / armor blue). No black panel.
  virtual Result<void> draw_health_ring(float x, float y, float radius,
                                        int health, int armor) = 0;
  virtual Result<void> draw_text(float x, float y, const char* text,
                                 uint32_t color, float size) = 0;
  virtual Result<void> draw_line(float x1, float y1, float x2, float y2,
                                 uint32_t color) = 0;
  /// Filled rectangle. (x,y) top-left, (w,h) width/height in normalized coords.
  virtual Result<void> draw_rect_filled(float x, float y, float w, float h,
                                        uint32_t color) = 0;
  /// Outlined rectangle. Thickness is informational (constant in this pipeline).
  virtual Result<void> draw_rect(float x, float y, float w, float h,
                                 uint32_t color, float thickness = 1.f) = 0;
  virtual Result<void> draw_crosshair(float x, float y, float size,
                                      uint32_t color) = 0;
  virtual Result<void> draw_radar_frame(const RadarFrame& frame) = 0;

  // Window messages
  virtual bool process_messages() = 0;  // returns false if quit requested
};

// ── Enum ──────────────────────────────────────────────────────────
// Windows research path is D3D11. None is the honest Null fallback when
// device creation fails or the platform has no GPU backend (never a
// pretend-success path that claims D3D11 while drawing nothing).
enum class GpuApi {
  D3D11,
  None,
};

// ── Factory ───────────────────────────────────────────────────────
Result<RenderPipeline*> create_render_pipeline(int width, int height,
                                               const char* title);
/// Create pipeline with floating overlay style (preferred for radar demos).
Result<RenderPipeline*> create_render_pipeline(const OverlayStyle& style,
                                               const char* title);

// ── Helpers ───────────────────────────────────────────────────────
/// Create transparent topmost overlay window (optionally layered alpha).
Result<void*> create_overlay_window(int width, int height, const char* title,
                                    bool topmost, bool transparent,
                                    bool clickthrough,
                                    std::uint8_t window_alpha = 255);
Result<void> destroy_overlay_window(void* hwnd);
/// Resolution-scaled SE nudge (equal +x/+y). Table calibrated around 800p=25.
int resolution_center_nudge_px(int client_h, int base_nudge_at_800p = 24);

/// Place HWND in a screen-work-area corner and re-assert topmost.
void place_overlay_corner(void* hwnd, int width, int height, int margin_px,
                          OverlayStyle::Corner corner, bool topmost);
/// Find CS2 game window (SDL_app / Valve001 / title match).
void* find_cs2_game_window();
/// Compute absolute screen rect of the in-game minimap from the game client.
InGameRadarLayout compute_ingame_radar_layout(void* game_hwnd,
                                              const OverlayStyle& style);
/// Move/resize overlay to sit exactly on the in-game radar.
bool place_overlay_on_ingame_radar(void* overlay_hwnd, const OverlayStyle& style,
                                   InGameRadarLayout* out_layout = nullptr);
void set_overlay_window_alpha(void* hwnd, std::uint8_t alpha);
/// Fully transparent chrome: DWM glass + black color-key (no black background).
void configure_overlay_transparency(void* hwnd, std::uint8_t content_alpha);
void assert_overlay_topmost(void* hwnd);
void set_overlay_bounds(void* hwnd, int x, int y, int w, int h, bool topmost);

// Educational / opt-in DXGI Output Duplication (desktop_dup_overlay demo).
// Real Windows path: CreateDXGIFactory1 → EnumAdapters/Outputs → IDXGIOutput1,
// then DuplicateOutput + AcquireNextFrame + staging-texture BGRA readback.
// Opt-in only — not forced into the live radar stealth path. Non-Windows
// builds return a controlled platform failure so demos still link.
Result<bool> desktop_duplication_supported();
Result<std::vector<std::uint8_t>> capture_desktop_frame(int& width, int& height);

}  // namespace real::gpu
