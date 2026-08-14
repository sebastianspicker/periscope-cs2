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

// ====================================================================
// HudRadarReader
// ====================================================================

bool HudRadarReader::read_element_name(uint64_t stringAddr, char* out,
    size_t outSize, bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    if (!stringAddr || !out || outSize == 0) return false;
    // Read string byte by byte up to outSize-1
    char buf[256];
    size_t toRead = std::min(sizeof(buf), outSize);
    if (!readFn(stringAddr, buf, toRead)) return false;
    buf[toRead - 1] = '\0';
    std::memcpy(out, buf, toRead);
    return out[0] != '\0';
}

int HudRadarReader::find_hud_tree_index(uint64_t treeBase, const char* targetName,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    // Read tree header
    HudTreeHeader header;
    if (!readFn(treeBase, &header, sizeof(header)))
        return -1;

    // Navigate BST starting from rootIndex
    int currentIndex = header.rootIndex;
    if (currentIndex < 0) return -1;

    size_t targetLen = std::strlen(targetName);

    while (currentIndex >= 0) {
        // Read element at index
        HudElement elem;
        uint64_t elemAddr = treeBase + sizeof(HudTreeHeader) +
                            static_cast<uint64_t>(currentIndex) * sizeof(HudElement);
        if (!readFn(elemAddr, &elem, sizeof(elem)))
            return -1;

        // Read element name
        char elemName[128];
        if (!read_element_name(elem.stringAddress, elemName, sizeof(elemName), readFn))
            return -1;

        // Case-insensitive comparison
        int cmp = 0;
        size_t pos = 0;
        while (targetName[pos] && elemName[pos]) {
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(targetName[pos])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(elemName[pos])));
            if (a != b) { cmp = (a < b) ? -1 : 1; break; }
            ++pos;
        }
        if (cmp == 0) {
            if (targetName[pos] == '\0' && elemName[pos] == '\0') {
                // Exact match — return element address
                return currentIndex;
            }
            cmp = (targetName[pos] == '\0') ? -1 : 1;
        }

        if (cmp < 0) {
            currentIndex = elem.leftIndex;
        } else {
            currentIndex = elem.rightIndex;
        }
    }

    return -1;
}

uint64_t HudRadarReader::read_hud_element_address(uint64_t cHudAddr, int index,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    // Read element count and data pointer
    uint32_t elementCount = 0;
    uint64_t elementData = 0;

    if (!readFn(cHudAddr + HudRadarOffsets::kHudElementCountOffset,
                &elementCount, sizeof(elementCount)))
        return 0;
    if (!readFn(cHudAddr + HudRadarOffsets::kHudElementDataOffset,
                &elementData, sizeof(elementData)))
        return 0;

    if (index < 0 || static_cast<uint32_t>(index) >= elementCount)
        return 0;

    // Read element address from array
    uint64_t elementAddr = 0;
    uint64_t arrayEntry = elementData + static_cast<uint64_t>(index) *
                          sizeof(uint64_t);
    if (!readFn(arrayEntry, &elementAddr, sizeof(elementAddr)))
        return 0;

    return elementAddr;
}

uint64_t HudRadarReader::find_hud_element_by_string_scan(
    uint64_t clientBase, size_t clientSize, const char* targetName,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    // Scan client module memory for the target name string
    size_t targetLen = std::strlen(targetName);
    if (targetLen == 0) return 0;

    constexpr size_t kScanChunk = 0x10000;
    std::vector<uint8_t> chunk(kScanChunk);

    std::vector<uint64_t> stringAddrs;

    // First pass: find all occurrences of the target string
    for (uint64_t offset = 0; offset < clientSize; offset += kScanChunk) {
        size_t readSize = kScanChunk;
        if (offset + readSize > clientSize)
            readSize = clientSize - offset;

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
            if (match) {
                stringAddrs.push_back(clientBase + offset + i);
            }
        }
    }

    if (stringAddrs.empty()) return 0;

    // Second pass: scan for pointers to these string addresses within the module
    for (uint64_t offset = 0; offset + 8 <= clientSize; offset += 1) {
        size_t readSize = std::min(kScanChunk, clientSize - offset);
        if (!readFn(clientBase + offset, chunk.data(), readSize))
            continue;

        for (size_t i = 0; i + 8 <= readSize; ++i) {
            uint64_t ptrVal = 0;
            std::memcpy(&ptrVal, chunk.data() + i, sizeof(ptrVal));

            for (auto strAddr : stringAddrs) {
                if (ptrVal == strAddr) {
                    // Found a pointer to the string — this might be a HudElement
                    // Check if this looks like a HudElement (has left/right indices)
                    uint64_t candidateAddr = clientBase + offset + i;

                    // Try to read as HudElement
                    HudElement elem;
                    if (readFn(candidateAddr - offsetof(HudElement, stringAddress),
                               &elem, sizeof(elem))) {
                        // Verify it has reasonable element address
                        if (elem.elementAddress > clientBase &&
                            elem.elementAddress < clientBase + clientSize * 2) {
                            return elem.elementAddress;
                        }
                    }

                    return candidateAddr;
                }
            }
        }
    }

    return 0;
}

uint64_t HudRadarReader::find_hud_element(uint64_t cHudAddr, const char* targetName,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    if (!cHudAddr || !targetName || !readFn) return 0;

    // Contiguous panel table on CHud (live-retuned 2026-07-26).
    // Try primary count/data, then alt pairs for build drift.
    const struct { uint64_t countOff; uint64_t dataOff; } kTablePairs[] = {
        {HudRadarOffsets::kHudElementCountOffset, HudRadarOffsets::kHudElementDataOffset},
        {HudRadarOffsets::kHudElementCountAlt0,   HudRadarOffsets::kHudElementDataAlt0},
        {HudRadarOffsets::kHudElementCountAlt1,   HudRadarOffsets::kHudElementDataAlt1},
    };

    for (const auto& pair : kTablePairs) {
        uint32_t elementCount = 0;
        uint64_t elementData = 0;
        if (!readFn(cHudAddr + pair.countOff, &elementCount, sizeof(elementCount)))
            continue;
        if (!readFn(cHudAddr + pair.dataOff, &elementData, sizeof(elementData)))
            continue;
        if (!elementData || elementCount == 0 || elementCount >= 512)
            continue;

        for (uint32_t i = 0; i < elementCount; ++i) {
            const uint64_t entry =
                elementData + static_cast<uint64_t>(i) * HudRadarOffsets::kPanelEntryStride;
            uint64_t strAddr = 0;
            uint64_t objAddr = 0;
            if (!readFn(entry + HudRadarOffsets::kPanelEntryNamePtr, &strAddr,
                        sizeof(strAddr)))
                continue;
            if (!readFn(entry + HudRadarOffsets::kPanelEntryObjectPtr, &objAddr,
                        sizeof(objAddr)))
                continue;
            if (!strAddr || !objAddr) continue;

            char elemName[128];
            if (!read_element_name(strAddr, elemName, sizeof(elemName), readFn))
                continue;
            if (std::strcmp(elemName, targetName) == 0) {
                return objAddr;  // panel object base (fields relative to this)
            }
        }
    }

    // Legacy BST path (older builds)
    uint64_t treeBase = 0;
    if (readFn(cHudAddr + HudRadarOffsets::kHudTreeOffset, &treeBase,
               sizeof(treeBase)) &&
        treeBase) {
        int index = find_hud_tree_index(treeBase, targetName, readFn);
        if (index >= 0) {
            uint64_t elementAddr = read_hud_element_address(cHudAddr, index, readFn);
            if (elementAddr) return elementAddr;
        }
    }

    return 0;
}

bool HudRadarReader::update_snapshot(uint64_t cHudAddr,
    uint64_t clientBase, size_t clientSize,
    bool (*readFn)(uint64_t addr, void* buf, size_t size),
    bool stringScanFallback) noexcept
{
    // Find CCSGO_HudRadar element
    uint64_t elementAddr = find_hud_element(cHudAddr, "CCSGO_HudRadar", readFn);

    if (!elementAddr && stringScanFallback) {
        // Full-module string scan (slow) — optional for offline tooling only.
        elementAddr = find_hud_element_by_string_scan(clientBase, clientSize,
                                                       "CCSGO_HudRadar", readFn);
    }

    if (!elementAddr) return false;

    uint64_t elementBase = elementAddr;

    // Field-by-field reads (layout retuned; packed struct was brittle).
    float map_x = 0.f, map_y = 0.f, map_z = 0.f;
    float visMax = 0.f, vis = 0.f, mapScale = 0.f, maxSq = 0.f, rscale = 0.f;
    float origin3[3]{};
    int32_t isRoundI = 1;

    readFn(elementBase + HudRadarOffsets::kRadarMapTexturePositionOffset, &map_x, 4);
    readFn(elementBase + HudRadarOffsets::kRadarMapTexturePositionOffset + 4, &map_y, 4);
    readFn(elementBase + HudRadarOffsets::kRadarLocalOrigin3Offset, origin3, 12);
    readFn(elementBase + HudRadarOffsets::kRadarVisibilitySizeMaxOffset, &visMax, 4);
    readFn(elementBase + HudRadarOffsets::kRadarVisibilitySizeOffset, &vis, 4);
    readFn(elementBase + HudRadarOffsets::kRadarMapTextureScaleOffset, &mapScale, 4);
    readFn(elementBase + HudRadarOffsets::kRadarMaxVisibilitySquaredOffset, &maxSq, 4);
    readFn(elementBase + HudRadarOffsets::kRadarScaleOffset, &rscale, 4);
    readFn(elementBase + HudRadarOffsets::kRadarIsRoundOffset, &isRoundI, 4);

    // Prefer full origin3 when it looks like world coords (always-centered radar).
    if (std::fabs(origin3[0]) > 1.f || std::fabs(origin3[1]) > 1.f) {
        map_x = origin3[0];
        map_y = origin3[1];
        map_z = origin3[2];
    }

    m_snapshot.isRound = (isRoundI != 0);
    m_snapshot.mapTexturePosition = {map_x, map_y, map_z};
    m_snapshot.visibilitySizeMax = visMax;
    m_snapshot.visibilitySize = vis;
    // mapTextureScale in polar_map is a multiply on world delta.
    // When panel stores a non-reciprocal scale-ish value, force 1.0 and use
    // world-space max visibility derived from cl_radar-like field.
    if (std::isfinite(mapScale) && mapScale > 0.f && mapScale < 0.1f) {
        m_snapshot.mapTextureScale = mapScale;  // true reciprocal
    } else {
        m_snapshot.mapTextureScale = 1.0f;  // world units
    }
    m_snapshot.originTexturePositionDifference = {0.f, 0.f, 0.f};

    // Edge radius: prefer maxSq if sane, else derive from radarScale / defaults.
    if (std::isfinite(maxSq) && maxSq > 100.f && maxSq < 1e10f &&
        m_snapshot.mapTextureScale < 0.1f) {
        m_snapshot.maxVisibilitySquared = maxSq;
    } else {
        // World-space edge: k / cl_radar_scale (k=750 @ scale 1.0).
        float cl = (std::isfinite(rscale) && rscale > 0.2f && rscale <= 1.05f)
                       ? rscale
                       : 0.7f;
        const float edge = 750.0f / cl;
        m_snapshot.maxVisibilitySquared = edge * edge;
    }

    m_snapshot.radarScale =
        (std::isfinite(rscale) && rscale > 0.f) ? rscale : probe_radar_scale(elementBase, readFn);

    // Soft validate: require map center + positive edge.
    m_snapshot.valid =
        (std::fabs(m_snapshot.mapTexturePosition.x) +
             std::fabs(m_snapshot.mapTexturePosition.y) >
         1.f) &&
        m_snapshot.maxVisibilitySquared > 1.f &&
        std::isfinite(m_snapshot.mapTextureScale) &&
        m_snapshot.mapTextureScale > 0.f;
    return m_snapshot.valid;
}

float HudRadarReader::probe_radar_scale(uint64_t elementBase,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    constexpr int32_t kCandidates[] = {
        0, 4, -4, 8, -8, 12, -12, 16, -16, 20, -20, 24
    };

    float bestValue = 0.7f;  // cl_radar_scale default
    float bestScore = -1.0f;

    float prevScale = m_snapshot.radarScale;
    if (prevScale <= 0.0f) prevScale = 0.7f;

    for (auto candidateOffset : kCandidates) {
        uint64_t addr = elementBase + HudRadarOffsets::kRadarScaleOffset +
                        candidateOffset;
        float value = 0.0f;
        if (!readFn(addr, &value, sizeof(value)))
            continue;

        if (!std::isfinite(value) || value <= 0.0f || value > 1000.0f)
            continue;

        // Score by continuity from previous frame
        float score = 1.0f / (std::abs(value - prevScale) + 0.01f);
        if (score > bestScore) {
            bestScore = score;
            bestValue = value;
        }
    }

    return bestValue;
}

} // namespace real::cs2::periscope

