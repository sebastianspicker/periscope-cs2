// periscope_overlay.hpp — Periscope-level stealth overlay.
//
// Stealth overlay window with:
//  - WS_EX_TOPMOST | WS_EX_APPWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT
//  - WS_POPUP style
//  - Alpha via D3D11 blending (not WS_EX_LAYERED)
//  - Randomized class name and window title per build (CLASS_SEED, BUILD_SALT)
//  - WDA_EXCLUDEFROMCAPTURE applied conditionally (only when streaming apps detected)
//  - Anti-screenshot XOR surface scramble
//  - CS2 window position following every 8 frames
//
// Reference: Periscope prototype/src/overlay/overlay.cpp (744 lines)

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <string>
#include <optional>

namespace real::gpu::periscope {

// ====================================================================
// Overlay configuration
// ====================================================================
struct OverlayConfig {
    int width{360};
    int height{360};
    std::string className;   // Generated from build seed
    std::string windowName;  // Generated from build seed
    uint8_t alpha{220};
    bool topmost{true};
    bool clickthrough{true};
};

// ====================================================================
// Periscope-level stealth overlay
// ====================================================================
class StealthOverlay {
public:
    bool initialize(const OverlayConfig& config) noexcept;
    void shutdown() noexcept;

    /// Reapply WDA_EXCLUDEFROMCAPTURE. Must be called every frame.
    bool ensure_capture_exclusion() noexcept;

    /// Follow CS2 window position. Call every ~8 frames.
    bool follow_window(uint64_t targetHwnd) noexcept;

    /// Set overlay position explicitly.
    void set_position(int x, int y, int w, int h) noexcept;

    /// Process window messages (returns false if WM_QUIT or ESC).
    bool process_messages() noexcept;

    /// Get native window handle.
    uint64_t handle() const noexcept { return m_hwnd; }

    bool is_initialized() const noexcept { return m_initialized; }
    int width() const noexcept { return m_config.width; }
    int height() const noexcept { return m_config.height; }

private:
    bool m_initialized{};
    uint64_t m_hwnd{};
    uint64_t m_classAtom{};  // V6-O2: Registered class ATOM for unregistration
    OverlayConfig m_config{};
    int m_followCounter{};

    bool register_window_class() noexcept;
    bool create_window() noexcept;

    // Anti-screenshot XOR scramble
    bool scramble_surface() noexcept;
    bool scramble_ensure_initialized() noexcept;
    void scramble_release() noexcept;

    // D3D11 scramble state — opaque PIMPL keeps <d3d11.h> out of the header.
    // Stored as void* so cpp-local helpers can own the concrete type.
    void* m_scramble{};
    bool m_scramble_armed{};   // Scramble requested (WDA failed to hide window)
    bool m_scramble_active{};  // Scramble rendering + presenting successfully
    uint64_t m_scramble_key{}; // Rolling XOR key — advances each frame
};

// ====================================================================
// Generate random class/window name from build seed.
// ====================================================================
std::string generate_random_name(uint64_t seed, const char* prefix) noexcept;

// ====================================================================
// DXGI Output Duplication composite
// ====================================================================
struct CompositeFrameMetrics {
    uint64_t acquireNs{};
    uint64_t copyNs{};
    uint64_t bytesCopied{};
    bool realDxgiPath{};  // true only if AcquireNextFrame was invoked
};

class DxgiComposite {
public:
    bool initialize() noexcept;
    void shutdown() noexcept;

    /// Acquire next desktop frame for timing mimic.
    /// Returns nullopt if platform doesn't support DXGI.
    std::optional<CompositeFrameMetrics> acquire_and_composite(
        int timeoutMs = 0) noexcept;

    bool is_initialized() const noexcept { return m_initialized; }

private:
    bool m_initialized{};
    bool m_pixelDataCaptured{};

    struct DxgiState;
    DxgiState* m_state{};  // PIMPL to avoid D3D11 includes in header

    bool create_device() noexcept;
    bool create_duplication() noexcept;
};

} // namespace real::gpu::periscope
