#include "real/gpu/render_pipeline.hpp"
#include "real/gpu/render_pipeline_internal.hpp"
#include "real/platform.hpp"
#include "real/win/xorstr.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#include "real/win/peb_util.hpp"
#if LR_COMPILER_MSVC
#include <intrin.h>
#endif
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwmapi.lib")
#endif

namespace real::gpu {

// ── Overlay window helpers ────────────────────────────────────────
int resolution_center_nudge_px(int client_h, int base_nudge_at_800p) {
  // Linear table around measured 800p=24: scale by client height / 800.
  if (client_h < 200) client_h = 800;
  if (base_nudge_at_800p < 0) base_nudge_at_800p = 24;
  const float n =
      static_cast<float>(base_nudge_at_800p) * (static_cast<float>(client_h) / 800.0f);
  return std::clamp(static_cast<int>(std::lround(n)), 0, 200);
}

void set_overlay_bounds(void* hwnd, int x, int y, int w, int h, bool topmost) {
#if LR_PLATFORM_WINDOWS
  if (!hwnd || w <= 0 || h <= 0) return;
  SetWindowPos(static_cast<HWND>(hwnd),
               topmost ? HWND_TOPMOST : HWND_NOTOPMOST, x, y, w, h,
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
  (void)hwnd;
  (void)x;
  (void)y;
  (void)w;
  (void)h;
  (void)topmost;
#endif
}

void place_overlay_corner(void* hwnd, int width, int height, int margin_px,
                          OverlayStyle::Corner corner, bool topmost) {
#if LR_PLATFORM_WINDOWS
  if (!hwnd || width <= 0 || height <= 0) return;
  RECT work{};
  if (!SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0)) {
    work.left = 0;
    work.top = 0;
    work.right = GetSystemMetrics(SM_CXSCREEN);
    work.bottom = GetSystemMetrics(SM_CYSCREEN);
  }
  const int m = margin_px < 0 ? 0 : margin_px;
  int x = work.left + m;
  int y = work.top + m;
  switch (corner) {
    case OverlayStyle::Corner::TopLeft:
      x = work.left + m;
      y = work.top + m;
      break;
    case OverlayStyle::Corner::TopRight:
      x = work.right - width - m;
      y = work.top + m;
      break;
    case OverlayStyle::Corner::BottomLeft:
      x = work.left + m;
      y = work.bottom - height - m;
      break;
    case OverlayStyle::Corner::BottomRight:
      x = work.right - width - m;
      y = work.bottom - height - m;
      break;
  }
  set_overlay_bounds(hwnd, x, y, width, height, topmost);
#else
  (void)hwnd;
  (void)width;
  (void)height;
  (void)margin_px;
  (void)corner;
  (void)topmost;
#endif
}

#if LR_PLATFORM_WINDOWS
namespace {
struct EnumCtx {
  DWORD pid = 0;
  HWND best = nullptr;
  int best_area = 0;
};

BOOL CALLBACK enum_cs2_windows(HWND hwnd, LPARAM lparam) {
  auto* ctx = reinterpret_cast<EnumCtx*>(lparam);
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (ctx->pid != 0 && pid != ctx->pid) return TRUE;
  if (!IsWindowVisible(hwnd)) return TRUE;

  char cls[64]{};
  char title[128]{};
  GetClassNameA(hwnd, cls, static_cast<int>(sizeof(cls)));
  GetWindowTextA(hwnd, title, static_cast<int>(sizeof(title)));

  const bool class_ok =
      std::strcmp(cls, "SDL_app") == 0 || std::strcmp(cls, "Valve001") == 0;
  const bool title_ok =
      std::strstr(title, "Counter-Strike") != nullptr ||
      std::strstr(title, "CS2") != nullptr;
  if (!class_ok && !title_ok) return TRUE;

  RECT rc{};
  if (!GetClientRect(hwnd, &rc)) return TRUE;
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;
  if (w < 200 || h < 200) return TRUE;
  const int area = w * h;
  if (area > ctx->best_area) {
    ctx->best_area = area;
    ctx->best = hwnd;
  }
  return TRUE;
}

float env_float(const char* name, float fallback) {
  const char* v = std::getenv(name);
  if (!v || !*v) return fallback;
  char* end = nullptr;
  const float f = std::strtof(v, &end);
  return (end && end != v) ? f : fallback;
}

int env_int(const char* name, int fallback) {
  const char* v = std::getenv(name);
  if (!v || !*v) return fallback;
  char* end = nullptr;
  const long n = std::strtol(v, &end, 10);
  return (end && end != v) ? static_cast<int>(n) : fallback;
}
}  // namespace
#endif

void* find_cs2_game_window() {
#if LR_PLATFORM_WINDOWS
  // Prefer class-based lookup (borderless / windowed: SDL_app).
  if (HWND h = FindWindowA("SDL_app", "Counter-Strike 2")) return h;
  if (HWND h = FindWindowA("SDL_app", nullptr)) {
    char title[128]{};
    GetWindowTextA(h, title, static_cast<int>(sizeof(title)));
    if (std::strstr(title, "Counter-Strike") || title[0] == '\0') return h;
  }
  if (HWND h = FindWindowA("Valve001", nullptr)) return h;

  // Exclusive fullscreen / multi-monitor: match largest visible window owned by cs2.exe.
  DWORD cs2_pid = 0;
  {
    HWND probe = FindWindowA("SDL_app", nullptr);
    if (probe) GetWindowThreadProcessId(probe, &cs2_pid);
  }
  if (cs2_pid == 0) {
    // Toolhelp fallback for exclusive FS when class lookup misses.
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
      PROCESSENTRY32 pe{};
      pe.dwSize = sizeof(pe);
      if (Process32First(snap, &pe)) {
        do {
          if (_stricmp(pe.szExeFile, "cs2.exe") == 0) {
            cs2_pid = pe.th32ProcessID;
            break;
          }
        } while (Process32Next(snap, &pe));
      }
      CloseHandle(snap);
    }
  }

  EnumCtx ctx{};
  ctx.pid = cs2_pid;
  EnumWindows(enum_cs2_windows, reinterpret_cast<LPARAM>(&ctx));
  if (ctx.best) return ctx.best;

  // Last resort: any large top-level window for the pid (exclusive FS).
  if (cs2_pid != 0) {
    EnumCtx any{};
    any.pid = cs2_pid;
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
      auto* c = reinterpret_cast<EnumCtx*>(lp);
      DWORD p = 0;
      GetWindowThreadProcessId(hwnd, &p);
      if (p != c->pid || !IsWindowVisible(hwnd)) return TRUE;
      RECT rc{};
      if (!GetClientRect(hwnd, &rc)) return TRUE;
      const int w = rc.right - rc.left;
      const int h = rc.bottom - rc.top;
      if (w < 320 || h < 240) return TRUE;
      const int area = w * h;
      if (area > c->best_area) {
        c->best_area = area;
        c->best = hwnd;
      }
      return TRUE;
    }, reinterpret_cast<LPARAM>(&any));
    return any.best;
  }
  return nullptr;
#else
  return nullptr;
#endif
}

InGameRadarLayout compute_ingame_radar_layout(void* game_hwnd,
                                              const OverlayStyle& style) {
  InGameRadarLayout out{};
#if LR_PLATFORM_WINDOWS
  if (!game_hwnd) return out;
  HWND game = static_cast<HWND>(game_hwnd);
  RECT client{};
  if (!GetClientRect(game, &client)) return out;
  POINT tl{0, 0};
  if (!ClientToScreen(game, &tl)) return out;

  const int cw = client.right - client.left;
  const int ch = client.bottom - client.top;
  if (cw < 200 || ch < 200) return out;

  out.client_w = cw;
  out.client_h = ch;
  out.game_hwnd = game_hwnd;

  // Manual overrides (calibration / env).
  const int force_size = style.force_size > 0
                             ? style.force_size
                             : env_int("LR_RADAR_SIZE", 0);
  const bool env_has_x = std::getenv("LR_RADAR_X") != nullptr;
  const bool env_has_y = std::getenv("LR_RADAR_Y") != nullptr;
  const int force_x = style.force_pos ? style.force_x
                                      : (env_has_x ? env_int("LR_RADAR_X", 0) : 0);
  const int force_y = style.force_pos ? style.force_y
                                      : (env_has_y ? env_int("LR_RADAR_Y", 0) : 0);
  const bool use_force_x = style.force_pos || env_has_x;
  const bool use_force_y = style.force_pos || env_has_y;

  float hud_scaling = env_float("LR_HUD_SCALING", style.hud_scaling);
  float cl_hud = env_float("LR_CL_HUD_RADAR_SCALE", style.cl_hud_radar_scale);
  float safe_x = env_float("LR_SAFEZONEX", style.safezonex);
  float safe_y = env_float("LR_SAFEZONEY", style.safezoney);
  float size_mul = env_float("LR_RADAR_SIZE_MUL", style.size_mul);
  hud_scaling = std::clamp(hud_scaling, 0.4f, 1.5f);
  cl_hud = std::clamp(cl_hud, 0.7f, 1.4f);
  safe_x = std::clamp(safe_x, 0.0f, 1.0f);
  safe_y = std::clamp(safe_y, 0.0f, 1.0f);
  size_mul = std::clamp(size_mul, 0.5f, 1.5f);

  // ── Resolution-aware size ──────────────────────────────────────
  // CS2 HUD is height-driven. Reference: 1920×1080, hud=1, cl_hud=1
  // → map disc ≈ 250px. Scale purely by client height so 800p / 1440p track.
  const float h_scale = static_cast<float>(ch) / 1080.0f;
  const float w_scale = static_cast<float>(cw) / 1920.0f;
  // Mild aspect blend: ultra-wide shouldn't balloon the radar.
  const float res_scale = h_scale * (0.85f + 0.15f * std::clamp(w_scale / h_scale, 0.75f, 1.25f));

  int size = 0;
  if (force_size > 0) {
    size = force_size;
  } else {
    // Disc diameter (map texture circle), not full chrome panel.
    // +2.5% live calibration so blips track in-game disc edge.
    const float disc =
        250.0f * 1.025f * res_scale * hud_scaling * cl_hud * size_mul;
    size = static_cast<int>(std::lround(disc));
  }
  size = std::clamp(size, 96, std::min(cw, ch) - 8);

  // ── Position: safezone + equal SE nudge (center alignment) ─────
  // Safezone shrinks the usable HUD rect from the edges toward center.
  const float pad_x = (1.0f - safe_x) * 0.5f * static_cast<float>(cw);
  const float pad_y = (1.0f - safe_y) * 0.5f * static_cast<float>(ch);

  // Small edge pad from the usable top-left corner (panel chrome).
  const int edge_inset = static_cast<int>(
      std::lround(2.0f * res_scale * hud_scaling * cl_hud));

  // Equal SE bias — resolution table (24 @ 800p) unless env/style override.
  int nudge = style.center_nudge_px;
  if (const int env_n = env_int("LR_RADAR_NUDGE", -1); env_n >= 0) {
    nudge = env_n;
  }
  if (nudge < 0) {
    nudge = resolution_center_nudge_px(ch, 24);
  } else if (style.center_nudge_px >= 23 && style.center_nudge_px <= 27) {
    // Treat calibrated base as 800p and scale with client height.
    nudge = resolution_center_nudge_px(ch, style.center_nudge_px);
  }
  nudge = std::clamp(nudge, 0, size);

  int inset_x =
      style.extra_inset_x >= 0 ? style.extra_inset_x
                               : env_int("LR_RADAR_INSET_X", edge_inset);
  int inset_y =
      style.extra_inset_y >= 0 ? style.extra_inset_y
                               : env_int("LR_RADAR_INSET_Y", edge_inset);

  // Equal SE bias: +nudge right AND +nudge down.
  int x = tl.x + static_cast<int>(std::lround(pad_x)) + inset_x + nudge;
  int y = tl.y + static_cast<int>(std::lround(pad_y)) + inset_y + nudge;
  if (use_force_x) x = force_x;
  if (use_force_y) y = force_y;

  // Keep fully inside the client.
  if (x + size > tl.x + cw) x = tl.x + cw - size;
  if (y + size > tl.y + ch) y = tl.y + ch - size;
  if (x < tl.x) x = tl.x;
  if (y < tl.y) y = tl.y;

  out.x = x;
  out.y = y;
  out.size = size;
  out.used_hud_scaling = hud_scaling;
  out.used_cl_hud_radar_scale = cl_hud;
  out.used_safezonex = safe_x;
  out.used_safezoney = safe_y;
  out.used_nudge = nudge;
  out.used_edge_inset = edge_inset;
  out.valid = true;
#else
  (void)game_hwnd;
  (void)style;
#endif
  return out;
}

bool place_overlay_on_ingame_radar(void* overlay_hwnd, const OverlayStyle& style,
                                   InGameRadarLayout* out_layout) {
#if LR_PLATFORM_WINDOWS
  void* game = find_cs2_game_window();
  InGameRadarLayout layout = compute_ingame_radar_layout(game, style);
  if (out_layout) *out_layout = layout;
  if (!overlay_hwnd || !layout.valid) return false;
  set_overlay_bounds(overlay_hwnd, layout.x, layout.y, layout.size, layout.size,
                     style.always_on_top);
  return true;
#else
  (void)overlay_hwnd;
  (void)style;
  if (out_layout) *out_layout = {};
  return false;
#endif
}

void set_overlay_window_alpha(void* hwnd, std::uint8_t alpha) {
  configure_overlay_transparency(hwnd, alpha);
}

void configure_overlay_transparency(void* hwnd, std::uint8_t content_alpha) {
#if LR_PLATFORM_WINDOWS
  if (!hwnd) return;
  HWND h = static_cast<HWND>(hwnd);
  LONG_PTR ex = GetWindowLongPtrA(h, GWL_EXSTYLE);
  SetWindowLongPtrA(h, GWL_EXSTYLE, ex | WS_EX_LAYERED);

  // Prefer color-key without DWM glass fill — DwmExtend(-1) + pure-black key
  // intermittently leaves an opaque dark radar plate on Win10/11.
  MARGINS margins{0, 0, 0, 0};
  DwmExtendFrameIntoClientArea(h, &margins);

  const BYTE a = content_alpha == 0 ? BYTE{1} : content_alpha;
  // Near-black magenta chroma key (never used by blips). Matches clear color.
  const COLORREF key = RGB(16, 0, 16);
  SetLayeredWindowAttributes(h, key, a, LWA_COLORKEY | LWA_ALPHA);
#else
  (void)hwnd;
  (void)content_alpha;
#endif
}

void assert_overlay_topmost(void* hwnd) {
#if LR_PLATFORM_WINDOWS
  if (!hwnd) return;
  SetWindowPos(static_cast<HWND>(hwnd), HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#else
  (void)hwnd;
#endif
}

Result<void*> create_overlay_window(int width, int height, const char* title,
                                    bool topmost, bool transparent, bool clickthrough,
                                    std::uint8_t window_alpha) {
#if LR_PLATFORM_WINDOWS
  // PEB-based HINSTANCE retrieval (avoids GetModuleHandleA hooking)
  auto* peb = real::win::peb::get_peb();
  HINSTANCE instance = peb ? static_cast<HINSTANCE>(peb->ImageBaseAddress) : nullptr;

  // Generate class name from OBF'd base + build key hash
  static const char* class_name = []() {
    static char buf[64];
    std::snprintf(buf, sizeof(buf), "%s_%016llX",
                  OBF("MpsSvc"),
                  static_cast<unsigned long long>(build::kXorKeySeed));
    return buf;
  }();

  WNDCLASSEXA window_class = {};
  window_class.cbSize = sizeof(window_class);
  window_class.lpfnWndProc = DefWindowProcA;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
  window_class.lpszClassName = class_name;
  if (!RegisterClassExA(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return os_error("RegisterClassEx");

  // Floating HUD: always-on-top + layered alpha + click-through + no focus steal.
  DWORD extended_style = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_LAYERED;
  if (topmost) extended_style |= WS_EX_TOPMOST;
  if (clickthrough) extended_style |= WS_EX_TRANSPARENT;
  // TOOLWINDOW keeps it off the taskbar for a "HUD" feel; APPWINDOW was used
  // earlier for anti-signature demos — prefer clean floating radar UX here.
  (void)transparent;

  // Default window title from OBF'd base + PRNG (never literal "Radar")
  static char default_title_buf[64];
  static bool title_init = false;
  if (!title_init) {
      uint64_t rng = __rdtsc();
      rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17;
      uint64_t t1 = rng;
      rng = rng * 0x5851F42D4C957F2DULL + 0x14057B7EF767814FULL;
      uint64_t t2 = rng;
      std::snprintf(default_title_buf, sizeof(default_title_buf), "%s_%016llX%016llX",
                    OBF("MpsSvc"), static_cast<unsigned long long>(t1), static_cast<unsigned long long>(t2));
      title_init = true;
  }

  HWND hwnd = CreateWindowExA(extended_style, class_name,
                              title ? title : default_title_buf,
                              WS_POPUP, 0, 0, width, height, nullptr, nullptr, instance, nullptr);
  if (!hwnd) return Result<void*>(nullptr, "CreateWindowEx failed");

  configure_overlay_transparency(hwnd, window_alpha);
  // Initial streamproof affinity (WDA_EXCLUDEFROMCAPTURE | WDA_MONITOR).
  {
    const DWORD wda = (1u << 4) | (1u << 0);
    auto& api = real::win::g_Api();
    if (api.resolved && api.SetWindowDisplayAffinity)
      api.SetWindowDisplayAffinity(hwnd, wda);
    else
      SetWindowDisplayAffinity(hwnd, wda);
  }
  ShowWindow(hwnd, SW_SHOWNA);
  if (topmost) {
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }
  return static_cast<void*>(hwnd);
#else
  (void)width;
  (void)height;
  (void)title;
  (void)topmost;
  (void)transparent;
  (void)clickthrough;
  (void)window_alpha;
  return Result<void*>(nullptr, "overlay windows require Windows");
#endif
}

Result<void> destroy_overlay_window(void* hwnd) {
#if LR_PLATFORM_WINDOWS
  if (hwnd && !DestroyWindow(static_cast<HWND>(hwnd))) return os_error("DestroyWindow");
#else
  (void)hwnd;
#endif
  return Result<void>();
}

}  // namespace real::gpu
