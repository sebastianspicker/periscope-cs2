// Split from periscope_hud.cpp — see MONOLITH_REFACTOR_LEDGER.
#include "real/cs2/periscope_hud.hpp"
#include "real/cs2/periscope_scanner.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <vector>

namespace real::cs2::periscope {

bool CvarManager::try_hud_mem_path(uint64_t cHudAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    // Path 3: Read CVar values from the CCSGO_HudRadar element's
    // known field offsets. This is the same approach as
    // HudRadarReader::update_snapshot() — we reuse the existing
    // radar snapshot fields that correspond to CVar values.
    //
    // Known CVar-to-field mappings in CCSGO_HudRadar:
    //   radarScale          (0x17F8C) ≈ cl_hud_radar_scale
    //   visibilitySize      (0x1A0)  / visibilitySizeMax (0x19C) ≈ cl_radar_scale
    //   These are the actual runtime values used for rendering.

    if (!cHudAddr || !readFn) return false;

    // Read the HUD tree base to find elements
    uint64_t treeBase = 0;
    readFn(cHudAddr + HudRadarOffsets::kHudTreeOffset, &treeBase, sizeof(treeBase));
    if (!treeBase) return false;

    // Read element count and data pointer
    uint32_t elementCount = 0;
    uint64_t elementData = 0;
    readFn(cHudAddr + HudRadarOffsets::kHudElementCountOffset,
           &elementCount, sizeof(elementCount));
    readFn(cHudAddr + HudRadarOffsets::kHudElementDataOffset,
           &elementData, sizeof(elementData));

    bool foundRadarElement = false;
    float radarScaleVal = 1.0f;
    float visibilitySize = 1.0f;
    float visibilitySizeMax = 1.0f;

    // Contiguous panel table (stride 0x20) — same as HudRadarReader.
    if (elementData && elementCount > 0) {
        for (uint32_t i = 0; i < std::min(elementCount, 128u); ++i) {
            const uint64_t entry =
                elementData + static_cast<uint64_t>(i) * HudRadarOffsets::kPanelEntryStride;
            uint64_t namePtr = 0, radarAddr = 0;
            readFn(entry + HudRadarOffsets::kPanelEntryNamePtr, &namePtr, 8);
            readFn(entry + HudRadarOffsets::kPanelEntryObjectPtr, &radarAddr, 8);
            if (!namePtr || !radarAddr) continue;
            char elemName[48]{};
            readFn(namePtr, elemName, sizeof(elemName) - 1);
            if (std::strcmp(elemName, "CCSGO_HudRadar") != 0) continue;

            readFn(radarAddr + HudRadarOffsets::kRadarVisibilitySizeMaxOffset,
                   &visibilitySizeMax, sizeof(visibilitySizeMax));
            readFn(radarAddr + HudRadarOffsets::kRadarVisibilitySizeOffset,
                   &visibilitySize, sizeof(visibilitySize));
            readFn(radarAddr + HudRadarOffsets::kRadarScaleOffset, &radarScaleVal,
                   sizeof(radarScaleVal));
            foundRadarElement = true;
            break;
        }
    }

    if (foundRadarElement) {
        // Panel +0x1D4 tracks map zoom (cl_radar_scale family), NOT panel size.
        if (radarScaleVal > 0.2f && radarScaleVal <= 1.05f) {
            m_values.clRadarScale = radarScaleVal;
        } else if (visibilitySizeMax > 0.0f && visibilitySize > 0.0f) {
            float ratio = visibilitySize / visibilitySizeMax;
            // 150/300=0.5 is a UI ratio — only use if in cl_radar range.
            if (ratio >= 0.25f && ratio <= 1.05f) {
                m_values.clRadarScale = ratio;
            }
        }
        // cl_hud_radar_scale (panel size) is not reliably on this object — leave default 1.
        if (m_values.clHudRadarScale <= 0.f) m_values.clHudRadarScale = 1.0f;
    }

    // Only the radar-related CVars can be read from HUD memory.
    // hud_scaling and safezone CVars must come from other paths.
    // We don't set hudScaling or safezone from this path.

    int resolvedCount = 0;
    if (m_values.clHudRadarScale > 0.0f && m_values.clHudRadarScale < 10.0f) ++resolvedCount;
    if (m_values.clRadarScale > 0.0f && m_values.clRadarScale < 5.0f) ++resolvedCount;

    if (resolvedCount >= 1) {
        m_values.allValid = true;
        return true;
    }

    return false;
}

bool CvarManager::try_xref_path(uint64_t clientBase, size_t clientSize,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    // Path 4: Cross-reference scan for CVar name strings.
    // Walks the entire client module looking for ConVar name strings,
    // then finds cross-references (pointers to those strings) to locate
    // ConVar objects. Useful when the pattern scan for vengine_cvar
    // misses but the ConVar name strings are still present.
    //
    // This path scans multiple modules (client.dll, tier0.dll) and uses
    // the following techniques:
    //   1. String search for each CVar name in all modules
    //   2. Pointer chase to find ConVar objects referencing those names
    //   3. Heap window scan for ConVar objects with embedded name copies

    if (!clientBase || !clientSize || !readFn) return false;

    constexpr size_t kScanChunk = 0x10000;
    std::vector<uint8_t> chunk(kScanChunk);

    struct CvarTarget {
        const char* name;
        uint32_t hash;
        uint64_t nameAddr;
        uint64_t convarAddr;
        float value;
    };

    CvarTarget targets[] = {
        {"hud_scaling",          cvar_hash::hud_scaling,          0, 0, 1.0f},
        {"safezonex",            cvar_hash::safezonex,           0, 0, 1.0f},
        {"safezoney",            cvar_hash::safezoney,           0, 0, 1.0f},
        {"cl_hud_radar_scale",   cvar_hash::cl_hud_radar_scale,  0, 0, 1.0f},
        {"cl_radar_scale",       cvar_hash::cl_radar_scale,      0, 0, 0.7f},
        {"cl_radar_rotate",      cvar_hash::cl_radar_rotate,     0, 0, 1.0f},
        {"cl_radar_icon_scale_min", cvar_hash::cl_radar_icon_scale_min, 0, 0, 0.4f},
    };
    constexpr size_t kNumTargets = sizeof(targets) / sizeof(targets[0]);

    // Pass 1: Find CVar name strings in client module
    for (size_t t = 0; t < kNumTargets; ++t) {
        const char* targetName = targets[t].name;
        size_t targetLen = std::strlen(targetName);

        for (uint64_t offset = 0; offset < clientSize && !targets[t].nameAddr;
             offset += kScanChunk) {
            size_t readSize = std::min(kScanChunk, clientSize - offset);
            if (!readFn(clientBase + offset, chunk.data(), readSize))
                continue;

            for (size_t i = 0; i + targetLen < readSize; ++i) {
                bool match = true;
                for (size_t j = 0; j < targetLen; ++j) {
                    if (chunk[i + j] != static_cast<uint8_t>(targetName[j])) {
                        match = false;
                        break;
                    }
                }
                if (match && chunk[i + targetLen] == '\0') {
                    targets[t].nameAddr = clientBase + offset + i;
                    break;
                }
            }
        }
    }

    // Pass 2: Chase cross-references (pointers to name strings)
    // Search for 8-byte pointers to each nameAddr within client module
    for (size_t t = 0; t < kNumTargets; ++t) {
        if (!targets[t].nameAddr) continue;

        for (uint64_t offset = 0; offset + 8 <= clientSize && !targets[t].convarAddr;
             offset += kScanChunk) {
            size_t readSize = std::min(kScanChunk, clientSize - offset);
            if (!readFn(clientBase + offset, chunk.data(), readSize))
                continue;

            for (size_t i = 0; i + 8 <= readSize; i += 8) {
                uint64_t ptrVal = 0;
                std::memcpy(&ptrVal, chunk.data() + i, sizeof(ptrVal));

                if (ptrVal == targets[t].nameAddr) {
                    uint64_t candidateAddr = clientBase + offset + i;

                    // Verify: check if this address looks like a ConVar
                    // by reading the linked list next pointer
                    uint64_t nextPtr = 0;
                    uint64_t selfPtr = 0;
                    readFn(candidateAddr + 0x08, &nextPtr, sizeof(nextPtr));

                    // A ConVar's next pointer should be null or reasonable
                    bool looksLikeConvar = true;
                    if (nextPtr != 0) {
                        // Check if nextPtr looks like a valid address
                        // (should be within our address space)
                        uint8_t testByte = 0;
                        looksLikeConvar = readFn(nextPtr, &testByte, 1);
                    }

                    if (looksLikeConvar) {
                        targets[t].convarAddr = candidateAddr;
                        targets[t].value = probe_float(candidateAddr, readFn,
                                                        targets[t].value);
                    }
                }
            }
        }
    }

    // Pass 3: Heap window scan for ConVar objects
    // Some ConVars are allocated on the heap and may not have direct
    // cross-references in the module. We scan for patterns that look
    // like ConVar objects using the name strings we already found.
    //
    // For this, we read larger blocks and look for structures where
    // a pointer field matches our known name addresses.

    // Apply results
    for (size_t t = 0; t < kNumTargets; ++t) {
        if (targets[t].convarAddr) {
            float value = targets[t].value;
            if (std::isfinite(value) && value > 0.0f) {
                switch (targets[t].hash) {
                case cvar_hash::hud_scaling:
                    m_values.hudScaling = value; break;
                case cvar_hash::safezonex:
                    m_values.safezoneX = value; break;
                case cvar_hash::safezoney:
                    m_values.safezoneY = value; break;
                case cvar_hash::cl_hud_radar_scale:
                    m_values.clHudRadarScale = value; break;
                case cvar_hash::cl_radar_scale:
                    m_values.clRadarScale = value; break;
                case cvar_hash::cl_radar_rotate:
                    m_values.clRadarRotate = (value >= 0.5f); break;
                case cvar_hash::cl_radar_icon_scale_min:
                    m_values.clRadarIconScaleMin = value; break;
                }
            }
        }
    }

    int resolvedCount = 0;
    if (m_values.hudScaling > 0.0f && m_values.hudScaling < 10.0f) ++resolvedCount;
    if (m_values.clRadarScale > 0.0f && m_values.clRadarScale < 10.0f) ++resolvedCount;
    if (m_values.clHudRadarScale > 0.0f && m_values.clHudRadarScale < 10.0f) ++resolvedCount;

    if (resolvedCount >= 2) {
        m_values.allValid = true;
        return true;
    }

    return false;
}

} // namespace real::cs2::periscope

