// periscope_scanner.hpp — Periscope-level pattern scanner.
//
// SIMD-accelerated pattern scanner with RIP-relative address resolution.
// Uses 10 validated patterns from Periscope's working prototype.
// All reads go through the hijack reader.
//
// Reference: Periscope prototype/src/cs2/offsets.hpp (313 lines)
//            Periscope prototype/src/cs2/offsets_patterns.cpp (42 lines)

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <optional>

namespace real::cs2::periscope {

// ====================================================================
// Pattern entry
// ====================================================================
struct PatternEntry {
    std::string name;
    std::vector<uint8_t> bytes;
    std::vector<uint8_t> mask;    // 0xFF = match, 0x00 = wildcard
    int ripOffset{};             // Offset from match to RIP-relative addr
    int instructionLen{};        // Total instruction length for RIP resolve
};

struct ScanResult {
    std::string name;
    uint64_t address{};
    bool found{};
};

// ====================================================================
// Resolved offsets matching Periscope's layout
// ====================================================================
struct PeriscopeOffsets {
    // Resolved globals
    uint64_t c_hud{};                // CHud base
    uint64_t c_hud_fallback1{};      // Fallback pattern 1
    uint64_t c_hud_fallback2{};      // Fallback pattern 2
    uint64_t c_hud_panel{};         // HUD panel
    uint64_t vengine_cvar{};        // CVar interface
    uint64_t window_width{};        // Engine2 window width address
    uint64_t window_height{};       // Engine2 window height address
    uint64_t entity_list{};         // Entity list
    uint64_t local_controller{};    // Local player controller
    uint64_t local_pawn{};          // Local player pawn

    // Module bases
    uint64_t client_dll{};
    uint64_t tier0_dll{};
    uint64_t engine2_dll{};

    // RVA fallbacks (when patterns miss)
    static constexpr uint64_t kDwEntityList            = 0x254EE60;
    static constexpr uint64_t kDwLocalPlayerPawn       = 0x23A4238;
    static constexpr uint64_t kDwLocalPlayerController = 0x237EBA0;
    static constexpr uint64_t kDwViewAngles            = 0x23B9C78;
    static constexpr uint64_t kDwPlantedC4             = 0x236E678;
    static constexpr uint64_t kDwCSGOInput             = 0x23B95F0;
    static constexpr uint64_t kDwWindowWidth           = 0x9118D0;

    bool valid{};

    bool is_complete() const noexcept {
        return client_dll != 0 && c_hud != 0 && entity_list != 0;
    }
};

// ====================================================================
// Schema field offsets (FNV-1a resolved, Periscope-validated)
// ====================================================================
struct SchemaOffsets {
    static constexpr uint64_t m_iHealth              = 0x34C;
    static constexpr uint64_t m_iTeamNum             = 0x3E7;  // uint8
    static constexpr uint64_t m_vOldOrigin           = 0x13B8;
    static constexpr uint64_t m_vecAbsOrigin         = 0xC8;
    static constexpr uint64_t m_angEyeAngles         = 0x3340;
    static constexpr uint64_t m_hPlayerPawn          = 0x914;
    static constexpr uint64_t m_pGameSceneNode       = 0x330;
    static constexpr uint64_t kIdentityStride        = 0x70;
    static constexpr uint64_t m_iWeaponID            = 0x10B0;
};

// ====================================================================
// HUD radar offsets (live-retuned 2026-07-26 against CS2 SDL build)
// ====================================================================
struct HudRadarOffsets {
    // CHud panel registry — primary layout (polar=1 in-match verified):
    // Contiguous panel table: count @ +0x260, data @ +0x268, stride 0x20.
    // Entry layout: [+0x00 pad][+0x10 name*][+0x18 panel_object*]
    static constexpr uint64_t kHudTreeOffset                 = 0x258; // legacy BST root*
    static constexpr uint64_t kHudElementCountOffset         = 0x260;
    static constexpr uint64_t kHudElementDataOffset          = 0x268;
    // Fallback table pairs if primary count/data miss (build drift).
    static constexpr uint64_t kHudElementCountAlt0           = 0x258;
    static constexpr uint64_t kHudElementDataAlt0            = 0x260;
    static constexpr uint64_t kHudElementCountAlt1           = 0x268;
    static constexpr uint64_t kHudElementDataAlt1            = 0x270;
    static constexpr uint64_t kPanelEntryStride              = 0x20;
    static constexpr uint64_t kPanelEntryNamePtr             = 0x10;
    static constexpr uint64_t kPanelEntryObjectPtr           = 0x18;
    static constexpr uint64_t kRadarElementBaseAdjust        = 0; // object ptr is direct

    // CCSGO_HudRadar panel object fields (live dump, polar path):
    // Local/map center (always-centered radar tracks local XY here):
    static constexpr uint64_t kRadarMapTexturePositionOffset = 0x1A4; // float x,y
    static constexpr uint64_t kRadarLocalOrigin3Offset       = 0x448; // float x,y,z (preferred)
    static constexpr uint64_t kRadarVisibilitySizeMaxOffset  = 0x19C; // ~300
    static constexpr uint64_t kRadarVisibilitySizeOffset     = 0x1A0; // ~150
    static constexpr uint64_t kRadarMapTextureScaleOffset    = 0x1B4; // scale-ish
    // max visibility: derive from cl_radar when field unreliable
    static constexpr uint64_t kRadarMaxVisibilitySquaredOffset = 0x1B8;
    static constexpr uint64_t kRadarOriginTexturePositionDifferenceOffset = 0x1D0;
    static constexpr uint64_t kRadarScaleOffset              = 0x1D4; // ~0.63 cl_radar-like
    static constexpr uint64_t kRadarIsRoundOffset            = 0x184; // often 1
};

// ====================================================================
// Functions
// ====================================================================

/// Get the 10 Periscope-validated patterns.
const std::vector<PatternEntry>& get_patterns() noexcept;

/// Scan all patterns against a remote module.
/// moduleBase: base address of the module in the target process
/// moduleSize: size of the module image
/// readFn: function to read remote memory (e.g., hijack reader callback)
std::vector<ScanResult> scan_all_patterns(
    uint64_t moduleBase,
    size_t moduleSize,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;

/// Resolve a single RIP-relative address.
uint64_t resolve_rip(uint64_t matchAddr, int ripOffset,
                     int instructionLen) noexcept;

} // namespace real::cs2::periscope
