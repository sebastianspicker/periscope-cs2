// Split from periscope_overlay.cpp — see MONOLITH_REFACTOR_LEDGER.
#include "real/gpu/periscope_overlay.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/timing.hpp"
#include "real/win/xorstr.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#if LR_COMPILER_MSVC
#include <intrin.h>
#endif
#pragma comment(lib, "d3dcompiler.lib")
#endif

#include <cstring>
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>

namespace real::gpu::periscope {

namespace {

// Window procedure for the overlay window
#if LR_PLATFORM_WINDOWS
LRESULT CALLBACK overlay_wnd_proc(HWND hwnd, UINT msg,
                                  WPARAM wparam, LPARAM lparam) {
    switch (msg) {
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        break;
    case WM_ERASEBKGND:
        return 1;  // Prevent flicker
    case WM_SETCURSOR:
        // V6-O1: Hide cursor on overlay hover to prevent cursor-change detection.
        // WS_EX_TRANSPARENT passes clicks through but WM_SETCURSOR still fires,
        // causing the cursor to visually become IDC_ARROW over the overlay.
        SetCursor(nullptr);
        return TRUE;
    }
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}
#endif

// Simple heuristic: check for OBS, Discord, or other streaming/recording
// apps by window class name. Returns true if a known streaming app is running.
bool is_streaming_app_running() noexcept {
    auto& api = real::win::g_Api();
    if (!api.resolved || !api.FindWindowA) return false;

    const char* streamingClasses[] = {
        OBF("OBSWindowClass"),
        OBF("Qt5QWindowIcon"),
        OBF("Chrome_WidgetWin_1"),
        OBF("ApplicationFrameWindow"),
        nullptr
    };

    for (const char** cls = streamingClasses; *cls; ++cls) {
        if (api.FindWindowA(*cls, nullptr)) {
            return true;
        }
    }
    return false;
}

} // namespace

// ====================================================================
// StealthOverlay implementation
// ====================================================================

bool StealthOverlay::initialize(const OverlayConfig& config) noexcept {
#if LR_PLATFORM_WINDOWS
    m_config = config;

    // Generate randomized class and window names if not explicitly set
    if (m_config.className.empty()) {
        // Unify with render_pipeline.cpp: OBF("MpsSvc") + "_" + hex(build_key)
        static char classNameBuf[64];
        std::snprintf(classNameBuf, sizeof(classNameBuf), "%s_%016llX",
                      OBF("MpsSvc"),
                      static_cast<unsigned long long>(build::kXorKeySeed));
        m_config.className = classNameBuf;
    }
    if (m_config.windowName.empty()) {
        m_config.windowName = m_config.className + OBF("_Wnd");
    }

    if (!register_window_class()) return false;
    if (!create_window()) return false;

    // Initial WDA application
    ensure_capture_exclusion();

    m_initialized = true;
    return true;
#else
    (void)config;
    return false;
#endif
}

void StealthOverlay::shutdown() noexcept {
#if LR_PLATFORM_WINDOWS
    // Release the D3D11 scramble device/swap-chain state first.
    scramble_release();
    if (m_hwnd) {
        DestroyWindow(reinterpret_cast<HWND>(m_hwnd));
        m_hwnd = 0;
    }
    // V6-O2: Unregister window class ATOM to prevent class name enumeration
    if (m_classAtom) {
        auto* peb = real::win::peb::get_peb();
        HMODULE hInst = peb ? reinterpret_cast<HMODULE>(peb->ImageBaseAddress) : nullptr;
        if (hInst) {
            UnregisterClassA(reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(m_classAtom)), hInst);
        }
        m_classAtom = 0;
    }
#endif
    m_initialized = false;
}

bool StealthOverlay::register_window_class() noexcept {
#if LR_PLATFORM_WINDOWS
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = overlay_wnd_proc;
    auto* peb = real::win::peb::get_peb();
    wc.hInstance = peb ? reinterpret_cast<HMODULE>(peb->ImageBaseAddress) : nullptr;
    wc.hCursor = nullptr;  // V6-O1: No cursor — prevents cursor-change detection
    wc.hbrBackground = nullptr;
    wc.lpszClassName = m_config.className.c_str();

    // V6-O2: Store ATOM for unregistration on shutdown
    ATOM atom = RegisterClassExA(&wc);
    if (!atom) {
        // Class may already be registered
        return true;
    }
    m_classAtom = static_cast<uint64_t>(atom);
    return true;
#else
    return false;
#endif
}

bool StealthOverlay::create_window() noexcept {
#if LR_PLATFORM_WINDOWS
    // Use a diverse style combination that doesn't match the cheat overlay signature.
    // - WS_EX_LAYERED is detectable -> use D3D11 alpha blending instead
    // - WS_EX_TOPMOST is needed for overlay -> keep
    // - WS_EX_TOOLWINDOW is a cheat signature -> replace with WS_EX_APPWINDOW
    // - WS_EX_NOACTIVATE prevents focus stealing -> keep
    // - WS_EX_TRANSPARENT enables click-through -> keep but obfuscate
    DWORD exStyle = WS_EX_TOPMOST | WS_EX_APPWINDOW | WS_EX_NOACTIVATE |
                    WS_EX_TRANSPARENT;

    HWND hwnd = CreateWindowExA(
        exStyle,
        m_config.className.c_str(),
        m_config.windowName.c_str(),
        WS_POPUP,
        0, 0,  // position — set later by follow_window
        m_config.width,
        m_config.height,
        nullptr,
        nullptr,
        []() -> HMODULE { auto* p = real::win::peb::get_peb(); return p ? reinterpret_cast<HMODULE>(p->ImageBaseAddress) : nullptr; }(),
        nullptr
    );

    if (!hwnd) return false;

    // Alpha is handled via D3D11 blending, not via WS_EX_LAYERED + SetLayeredWindowAttributes.

    m_hwnd = reinterpret_cast<uint64_t>(hwnd);
    return true;
#else
    return false;
#endif
}

bool StealthOverlay::ensure_capture_exclusion() noexcept {
#if LR_PLATFORM_WINDOWS
    if (!m_hwnd) return false;

    static bool wda_applied = false;
    static uint64_t next_check_tsc = 0;

    if (wda_applied) {
        // Periodic re-check with jitter (60-120 seconds)
        unsigned int aux;
        uint64_t now = __rdtscp(&aux);
        if (now < next_check_tsc) return true;
    }

    // Only apply WDA if streaming apps are detected (not always).
    // This reduces the signal left by the WDA flag on the window.
    if (!is_streaming_app_running()) {
        return true;
    }

    // Compute WDA flags at runtime from individual bits instead of hardcoding.
    // WDA_EXCLUDEFROMCAPTURE = (1 << 4), WDA_MONITOR = (1 << 0)
    DWORD wdaFlags = (1 << 0) | (1 << 4);

    auto& api = real::win::g_Api();

    // Check current WDA state before applying to avoid redundant calls
    if (api.resolved && api.GetWindowDisplayAffinity) {
        DWORD currentFlags = 0;
        if (api.GetWindowDisplayAffinity(reinterpret_cast<HWND>(m_hwnd), &currentFlags) &&
            currentFlags == wdaFlags) {
            wda_applied = true;
            // WDA already hides the window from capture — no scramble needed.
            m_scramble_armed = false;
            m_scramble_active = false;
            uint64_t tsc_freq = 2500000000ULL;
            unsigned int aux;
            uint64_t jitter = (__rdtscp(&aux) % 61) * tsc_freq;
            next_check_tsc = __rdtscp(&aux) + (60ULL * tsc_freq) + jitter;
            return true;
        }
    }

    if (api.resolved && api.SetWindowDisplayAffinity) {
        BOOL result = api.SetWindowDisplayAffinity(
            reinterpret_cast<HWND>(m_hwnd), wdaFlags);
        if (result) {
            wda_applied = true;
            // WDA hides the window from capture — the scramble is not needed.
            m_scramble_armed = false;
            m_scramble_active = false;
            uint64_t tsc_freq = 2500000000ULL;
            unsigned int aux;
            uint64_t jitter = (__rdtscp(&aux) % 61) * tsc_freq;
            next_check_tsc = __rdtscp(&aux) + (60ULL * tsc_freq) + jitter;
        } else {
            // WDA failed — the XOR scramble is the fallback defense.
            m_scramble_armed = true;
            (void)scramble_surface();
        }
        return result != FALSE;
    }
    // Fallback: direct call
    BOOL result = SetWindowDisplayAffinity(
        reinterpret_cast<HWND>(m_hwnd), wdaFlags);
    if (result) {
        wda_applied = true;
        // WDA hides the window from capture — the scramble is not needed.
        m_scramble_armed = false;
        m_scramble_active = false;
        uint64_t tsc_freq = 2500000000ULL;
        unsigned int aux;
        uint64_t jitter = (__rdtscp(&aux) % 61) * tsc_freq;
        next_check_tsc = __rdtscp(&aux) + (60ULL * tsc_freq) + jitter;
    } else {
        // WDA failed — the XOR scramble is the fallback defense.
        m_scramble_armed = true;
        (void)scramble_surface();
    }
    return result != FALSE;
#else
    return false;
#endif
}

bool StealthOverlay::follow_window(uint64_t targetHwnd) noexcept {
#if LR_PLATFORM_WINDOWS
    if (!m_hwnd || !targetHwnd) return false;

    ++m_followCounter;
    // Randomize window follow interval to avoid periodic detection.
    // Use XOR-shift to produce non-deterministic intervals between 8-15 frames.
    uint64_t rngState = static_cast<uint64_t>(m_followCounter) * 0x9E3779B97F4A7C15ULL;
    rngState ^= rngState >> 12;
    rngState ^= rngState << 25;
    rngState ^= rngState >> 27;
    int interval = 8 + static_cast<int>(rngState & 7);
    if (m_followCounter % interval != 0) return true;

    RECT clientRect{};
    if (!GetClientRect(reinterpret_cast<HWND>(targetHwnd), &clientRect)) {
        return false;
    }

    POINT topLeft{};
    ClientToScreen(reinterpret_cast<HWND>(targetHwnd), &topLeft);

    // Random position jitter (1-2px) to avoid exact alignment
    rngState = rngState * 0x5851F42D4C957F2DULL + 0x14057B7EF767814FULL;
    int jitterX = (static_cast<int>(rngState & 1) * 2 - 1) * (1 + static_cast<int>((rngState >> 1) & 1));
    rngState = rngState * 0x5851F42D4C957F2DULL + 0x14057B7EF767814FULL;
    int jitterY = (static_cast<int>(rngState & 1) * 2 - 1) * (1 + static_cast<int>((rngState >> 1) & 1));

    // Overlay position matches target window
    SetWindowPos(
        reinterpret_cast<HWND>(m_hwnd),
        HWND_TOPMOST,
        topLeft.x + (clientRect.right - clientRect.left - m_config.width) / 2 + jitterX,
        topLeft.y + (clientRect.bottom - clientRect.top - m_config.height) / 2 + jitterY,
        m_config.width,
        m_config.height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    );

    return true;
#else
    (void)targetHwnd;
    return false;
#endif
}

void StealthOverlay::set_position(int x, int y, int w, int h) noexcept {
#if LR_PLATFORM_WINDOWS
    if (!m_hwnd) return;
    SetWindowPos(reinterpret_cast<HWND>(m_hwnd), HWND_TOPMOST,
                 x, y, w, h, SWP_NOACTIVATE);
    m_config.width = w;
    m_config.height = h;
#else
    (void)x; (void)y; (void)w; (void)h;
#endif
}

bool StealthOverlay::process_messages() noexcept {
#if LR_PLATFORM_WINDOWS
    if (!m_hwnd) return true;

    MSG msg{};
    while (PeekMessageA(&msg, reinterpret_cast<HWND>(m_hwnd),
                        0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return true;
#else
    return true;
#endif
}

} // namespace real::gpu::periscope

