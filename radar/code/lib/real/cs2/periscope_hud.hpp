// periscope_hud.hpp — Periscope-level HUD radar snapshot reader.
//
// Navigates CS2's HUD binary search tree to find "CCSGO_HudRadar",
// then reads 8+ snapshot fields needed for coordinate math.
//
// Tree walk: BST traversal comparing node names case-insensitively.
// Fallback: linear scan of element array if BST fails.
// String-scan fallback: if c_hud pattern misses, scan module for
// "CCSGO_HudRadar" string constant and chase pointers.
//
// Reference: Periscope prototype/src/cs2/hud_radar.cpp (343 lines)
//            Periscope prototype/99-radar.txt (403 lines)

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include <cmath>
#include <cstdint>

namespace real::cs2::periscope {

// Shared math types (also declared in periscope_entity.hpp with the same layout).
#ifndef LR_PERISCOPE_MATH_TYPES
#define LR_PERISCOPE_MATH_TYPES
struct Vector3 {
    float x{}, y{}, z{};
    float length_sqr() const noexcept { return x * x + y * y + z * z; }
};

struct Vector2 {
    float x{}, y{};
};
#endif

// ====================================================================
// HudRadar snapshot (8 fields from CCSGO_HudRadar - 32)
// ====================================================================
struct HudRadarSnapshot {
    bool isRound{};
    Vector3 mapTexturePosition{};
    float visibilitySizeMax{};
    float visibilitySize{};
    float mapTextureScale{};       // stored reciprocal: 1.0 / scale
    float maxVisibilitySquared{};
    Vector3 originTexturePositionDifference{};
    float radarScale{};
    bool valid{};

    bool validate() const noexcept {
        if (visibilitySizeMax <= 0.0f || visibilitySize <= 0.0f) return false;
        if (!std::isfinite(mapTextureScale) || mapTextureScale <= 0.0f) return false;
        if (!std::isfinite(radarScale) || radarScale <= 0.0f) return false;
        return mapTexturePosition.length_sqr() > 1.0f;
    }
};

// ====================================================================
// HUD tree structures
// ====================================================================
#pragma pack(push, 1)
struct HudTreeHeader {
    uint8_t pad[0xC]{};
    int32_t flags{};
    uint64_t data{};
    int32_t rootIndex{};
};

struct HudElement {
    int32_t leftIndex{};
    int32_t rightIndex{};
    uint8_t pad[0x8]{};
    uint64_t stringAddress{};
    uint64_t elementAddress{};
};
#pragma pack(pop)

// ====================================================================
// HudRadar reader
// ====================================================================
class HudRadarReader {
public:
    /// Find CCSGO_HudRadar via BST walk and read snapshot.
    /// cHudAddr: resolved address of CHud object.
    /// clientBase: base of client.dll (for string-scan fallback)
    /// clientSize: size of client.dll
    /// readFn: hijack reader callback
    /// stringScanFallback: set false for interactive demos (full client scan is slow).
    bool update_snapshot(uint64_t cHudAddr,
                         uint64_t clientBase, size_t clientSize,
                         bool (*readFn)(uint64_t addr, void* buf,
                                        size_t size),
                         bool stringScanFallback = true) noexcept;

    const HudRadarSnapshot& snapshot() const noexcept { return m_snapshot; }

    /// Probe radar scale at multiple offsets (12 candidates).
    float probe_radar_scale(uint64_t elementBase,
                            bool (*readFn)(uint64_t addr, void* buf,
                                           size_t size)) noexcept;

private:
    HudRadarSnapshot m_snapshot{};

    // BST walk
    int find_hud_tree_index(uint64_t treeBase, const char* targetName,
                            bool (*readFn)(uint64_t addr, void* buf,
                                           size_t size)) noexcept;
    uint64_t read_hud_element_address(uint64_t cHudAddr, int index,
                                      bool (*readFn)(uint64_t addr,
                                                     void* buf,
                                                     size_t size)) noexcept;
    uint64_t find_hud_element(uint64_t cHudAddr, const char* targetName,
                              bool (*readFn)(uint64_t addr, void* buf,
                                             size_t size)) noexcept;
    bool read_element_name(uint64_t stringAddr, char* out, size_t outSize,
                           bool (*readFn)(uint64_t addr, void* buf,
                                          size_t size)) noexcept;

    // String-scan fallback
    uint64_t find_hud_element_by_string_scan(
        uint64_t clientBase, size_t clientSize, const char* targetName,
        bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept;
};

// ====================================================================
// CVar reader (4-tier resolution cascade)
// ====================================================================
struct CvarInputs {
    float hudScaling{1.0f};
    float safezoneX{1.0f};
    float safezoneY{1.0f};
    float clHudRadarScale{1.0f};
    float clRadarScale{0.7f};
    bool clRadarRotate{true};
    float clRadarIconScaleMin{0.4f};
    bool allValid{};
    int resolutionPath{};  // 1-4 which path succeeded
};

class CvarManager {
public:
    /// Initialize: try paths in order (warm cache -> list -> hud -> xref).
    bool initialize(uint64_t engine2Base, size_t engine2Size,
                    uint64_t tier0Base, size_t tier0Size,
                    uint64_t clientBase, size_t clientSize,
                    uint64_t cHudAddr,
                    bool (*readFn)(uint64_t addr, void* buf,
                                   size_t size)) noexcept;

    const CvarInputs& values() const noexcept { return m_values; }

    /// Get single CVar by hash.
    template<typename T>
    T get(uint32_t hash, T fallback = T{}) const noexcept;

private:
    CvarInputs m_values{};

    bool try_warm_cache_path() noexcept;
    bool try_list_path(uint64_t tier0Base, size_t tier0Size,
                        uint64_t clientBase, size_t clientSize,
                        bool (*readFn)(uint64_t addr, void* buf,
                                       size_t size)) noexcept;
    bool try_hud_mem_path(uint64_t cHudAddr,
                          bool (*readFn)(uint64_t addr, void* buf,
                                         size_t size)) noexcept;
    bool try_xref_path(uint64_t clientBase, size_t clientSize,
                       bool (*readFn)(uint64_t addr, void* buf,
                                      size_t size)) noexcept;

    float probe_float(uint64_t convarAddr,
                      bool (*readFn)(uint64_t addr, void* buf,
                                     size_t size),
                      float fallback) noexcept;
};

// ====================================================================
// CVar name hashes (FNV-1a 32-bit)
// ====================================================================
namespace cvar_hash {
    constexpr uint32_t fnv1a(const char* str) noexcept {
        uint32_t hash = 0x811C9DC5u;
        while (*str) {
            hash ^= static_cast<uint32_t>(*str++);
            hash *= 0x01000193u;
        }
        return hash;
    }
    constexpr uint32_t hud_scaling          = fnv1a("hud_scaling");
    constexpr uint32_t safezonex            = fnv1a("safezonex");
    constexpr uint32_t safezoney            = fnv1a("safezoney");
    constexpr uint32_t cl_hud_radar_scale   = fnv1a("cl_hud_radar_scale");
    constexpr uint32_t cl_radar_scale       = fnv1a("cl_radar_scale");
    constexpr uint32_t cl_radar_rotate      = fnv1a("cl_radar_rotate");
    constexpr uint32_t cl_radar_icon_scale_min = fnv1a("cl_radar_icon_scale_min");
}

} // namespace real::cs2::periscope
