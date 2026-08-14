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
// CvarManager
// ====================================================================

float CvarManager::probe_float(uint64_t convarAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size),
    float fallback) noexcept
{
    if (!convarAddr) return fallback;

    // Try multiple offsets to find the float value within a ConVar struct.
    // The ConVar object has this approximate layout:
    //   +0x00 vtable     (8 bytes, pointer)
    //   +0x08 next       (8 bytes, linked list)
    //   +0x10 prev       (8 bytes)
    //   +0x18 name_ptr   (8 bytes)
    //   +0x20 desc_ptr   (8 bytes)
    //   +0x28 value      (4 bytes, float)  ← most likely
    //   +0x2C default    (4 bytes, float)
    //   +0x30 min        (4 bytes, float)
    //   +0x34 max        (4 bytes, float)
    //   +0x38 flags      (4 bytes, int)
    // Given this layout, offsets 0x28-0x34 are the most likely candidates.
    // We scan with weighted preference.

    struct ProbeEntry { uint32_t offset; float weight; };
    constexpr ProbeEntry kProbes[] = {
        {0x28, 3.0f},  // value (highest weight)
        {0x2C, 2.5f},  // default value
        {0x30, 1.5f},  // min
        {0x34, 1.5f},  // max
        {0x38, 0.5f},  // flags (unlikely to be float but could alias)
        {0x24, 0.5f},  // before value
        {0x20, 0.3f},  // desc pointer (should not be float)
        {0x18, 0.2f},  // name pointer (should not be float)
        {0x10, 0.1f},  // prev pointer
        {0x08, 0.1f},  // next pointer
    };

    float bestValue = fallback;
    float bestScore = -1.0f;

    for (auto [offset, weight] : kProbes) {
        float value = 0.0f;
        if (!readFn(convarAddr + offset, &value, sizeof(value)))
            continue;
        if (!std::isfinite(value)) continue;

        // Score: prefer reasonable CVar float values with offset weight
        float score = 0.0f;
        if (value > 0.0f && value < 10.0f) {
            score = 10.0f * weight;  // Most CVar floats are in this range
        } else if (value >= 0.0f && value < 100.0f) {
            score = 5.0f * weight;
        } else if (value >= -1000.0f && value < 1000.0f) {
            score = 1.0f * weight;
        }

        if (score > bestScore) {
            bestScore = score;
            bestValue = value;
        }
    }

    // Only return a value if we found a reasonable candidate (score > 0)
    return (bestScore > 0.0f) ? bestValue : fallback;
}

bool CvarManager::try_warm_cache_path() noexcept {
    // Path 1: Use previously cached values (always succeeds if already set)
    return m_values.allValid;
}

namespace {

// Resolve an export from a *remote* module via PE EAT (small RPM reads only —
// not a full-module PE/AOB string scan).
uint64_t resolve_remote_export(uint64_t moduleBase, size_t moduleSize,
                               const char* funcName,
                               bool (*readFn)(uint64_t, void*, size_t)) noexcept {
    if (!moduleBase || moduleSize < 0x200 || !funcName || !readFn) return 0;

    uint16_t magic = 0;
    if (!readFn(moduleBase, &magic, 2) || magic != 0x5A4D) return 0;  // MZ

    int32_t e_lfanew = 0;
    if (!readFn(moduleBase + 0x3C, &e_lfanew, 4) || e_lfanew <= 0 ||
        static_cast<size_t>(e_lfanew) + 0x100 > moduleSize)
        return 0;

    uint32_t peSig = 0;
    if (!readFn(moduleBase + static_cast<uint64_t>(e_lfanew), &peSig, 4) ||
        peSig != 0x00004550)
        return 0;  // PE\0\0

    // OptionalHeader.DataDirectory[0] (export) at +0x88 from PE for PE32+
    // NT headers: sig(4) + FileHeader(20) + OptionalHeader starts at +24
    // Magic PE32+ = 0x20B at OptionalHeader+0; Export dir VA/Size at Optional+0x70
    uint16_t optMagic = 0;
    if (!readFn(moduleBase + static_cast<uint64_t>(e_lfanew) + 24, &optMagic, 2))
        return 0;
    if (optMagic != 0x20B) return 0;  // PE32+ only

    uint32_t exportRva = 0, exportSize = 0;
    const uint64_t dirBase =
        moduleBase + static_cast<uint64_t>(e_lfanew) + 24 + 0x70;  // DataDirectory[0]
    if (!readFn(dirBase, &exportRva, 4) || !readFn(dirBase + 4, &exportSize, 4))
        return 0;
    if (!exportRva || !exportSize || exportRva >= moduleSize) return 0;

    // IMAGE_EXPORT_DIRECTORY
    uint32_t numberOfNames = 0, addrOfFunctions = 0, addrOfNames = 0,
             addrOfNameOrdinals = 0;
    const uint64_t exp = moduleBase + exportRva;
    if (!readFn(exp + 0x18, &numberOfNames, 4)) return 0;
    if (!readFn(exp + 0x1C, &addrOfFunctions, 4)) return 0;
    if (!readFn(exp + 0x20, &addrOfNames, 4)) return 0;
    if (!readFn(exp + 0x24, &addrOfNameOrdinals, 4)) return 0;
    if (!numberOfNames || numberOfNames > 65536) return 0;

    const size_t nameLen = std::strlen(funcName);
    for (uint32_t i = 0; i < numberOfNames; ++i) {
        uint32_t nameRva = 0;
        if (!readFn(moduleBase + addrOfNames + static_cast<uint64_t>(i) * 4,
                    &nameRva, 4) ||
            !nameRva || nameRva >= moduleSize)
            continue;
        char name[96]{};
        if (!readFn(moduleBase + nameRva, name, sizeof(name) - 1)) continue;
        if (std::strncmp(name, funcName, nameLen) != 0 || name[nameLen] != '\0')
            continue;

        uint16_t ordinal = 0;
        if (!readFn(moduleBase + addrOfNameOrdinals + static_cast<uint64_t>(i) * 2,
                    &ordinal, 2))
            return 0;
        uint32_t funcRva = 0;
        if (!readFn(moduleBase + addrOfFunctions + static_cast<uint64_t>(ordinal) * 4,
                    &funcRva, 4) ||
            !funcRva || funcRva >= moduleSize)
            return 0;
        // Skip forwarded exports (RVA inside export dir)
        if (funcRva >= exportRva && funcRva < exportRva + exportSize) return 0;
        return moduleBase + funcRva;
    }
    return 0;
}

// Follow short JMP / JMP [rip] thunks (export often jumps to real body).
uint64_t follow_code_thunk(uint64_t fn, bool (*readFn)(uint64_t, void*, size_t),
                           int depth = 0) noexcept {
    if (!fn || !readFn || depth > 4) return fn;
    uint8_t b[16]{};
    if (!readFn(fn, b, sizeof(b))) return fn;
    // E9 rel32  jmp
    if (b[0] == 0xE9) {
        int32_t rel = 0;
        std::memcpy(&rel, b + 1, 4);
        return follow_code_thunk(fn + 5 + rel, readFn, depth + 1);
    }
    // FF 25 xx xx xx xx  jmp [rip+disp]  (or EB short)
    if (b[0] == 0xFF && b[1] == 0x25) {
        int32_t disp = 0;
        std::memcpy(&disp, b + 2, 4);
        uint64_t slot = fn + 6 + disp;
        uint64_t target = 0;
        if (readFn(slot, &target, 8) && target)
            return follow_code_thunk(target, readFn, depth + 1);
    }
    if (b[0] == 0xEB) {
        int8_t rel = static_cast<int8_t>(b[1]);
        return follow_code_thunk(fn + 2 + rel, readFn, depth + 1);
    }
    return fn;
}

bool interface_reg_looks_valid(uint64_t head,
                               bool (*readFn)(uint64_t, void*, size_t)) noexcept {
    auto looks_like_user_ptr = [](uint64_t p) {
        return p >= 0x10000ULL && p < 0x00007FFFFFFFFFFFULL;
    };
    if (!looks_like_user_ptr(head)) return false;
    uint64_t createFn = 0, namePtr = 0, nextPtr = 0;
    if (!readFn(head + 0x00, &createFn, 8)) return false;
    if (!readFn(head + 0x08, &namePtr, 8)) return false;
    if (!looks_like_user_ptr(createFn) || !looks_like_user_ptr(namePtr)) return false;
    char nm[48]{};
    if (!readFn(namePtr, nm, sizeof(nm) - 1) || nm[0] < 33 || nm[0] > 126)
        return false;
    // Interface names: start with letter (VEngineCvar007, Source2EngineToClient, …)
    if (!((nm[0] >= 'A' && nm[0] <= 'Z') || (nm[0] >= 'a' && nm[0] <= 'z')))
        return false;
    // Prefer names that look like Source interface version strings
    bool hasDigit = false;
    for (int i = 0; nm[i]; ++i) {
        if (nm[i] >= '0' && nm[i] <= '9') hasDigit = true;
    }
    readFn(head + 0x10, &nextPtr, 8);
    if (nextPtr && !looks_like_user_ptr(nextPtr)) return false;
    (void)hasDigit;
    return true;
}

// Walk CreateInterface prologue for RIP-relative loads → candidate InterfaceReg heads.
// Source InterfaceReg: { createFn* +0x00, name* +0x08, next* +0x10 }
uint64_t find_interface_reg_head(uint64_t createInterfaceFn,
                                 bool (*readFn)(uint64_t, void*, size_t)) noexcept {
    if (!createInterfaceFn || !readFn) return 0;
    createInterfaceFn = follow_code_thunk(createInterfaceFn, readFn);
    uint8_t code[0x200]{};
    if (!readFn(createInterfaceFn, code, sizeof(code))) return 0;

    auto looks_like_user_ptr = [](uint64_t p) {
        return p >= 0x10000ULL && p < 0x00007FFFFFFFFFFFULL;
    };

    // Collect candidate heads; prefer ones whose first name is VEngineCvar*
    uint64_t fallback = 0;
    for (size_t i = 0; i + 7 < sizeof(code); ++i) {
        const uint8_t b0 = code[i];
        const uint8_t b1 = code[i + 1];
        const uint8_t b2 = code[i + 2];
        const bool rex = (b0 == 0x48 || b0 == 0x4C);
        if (!rex) continue;
        const bool isMov = (b1 == 0x8B);
        const bool isLea = (b1 == 0x8D);
        if (!isMov && !isLea) continue;
        if ((b2 & 0xC7) != 0x05) continue;

        int32_t disp = 0;
        std::memcpy(&disp, code + i + 3, 4);
        const uint64_t rip = createInterfaceFn + static_cast<uint64_t>(i) + 7;
        const uint64_t target = rip + static_cast<int64_t>(disp);

        // Candidate A: direct node pointer
        uint64_t heads[2] = {0, 0};
        if (isLea) {
            heads[0] = target;
            uint64_t thru = 0;
            if (readFn(target, &thru, 8) && thru) heads[1] = thru;
        } else {
            uint64_t loaded = 0;
            if (!readFn(target, &loaded, 8) || !loaded) continue;
            heads[0] = loaded;
            // double indirection
            uint64_t thru = 0;
            if (readFn(loaded, &thru, 8) && thru) heads[1] = thru;
        }

        for (uint64_t head : heads) {
            if (!head || !looks_like_user_ptr(head)) continue;
            if (!interface_reg_looks_valid(head, readFn)) continue;
            uint64_t namePtr = 0;
            char nm[48]{};
            readFn(head + 0x08, &namePtr, 8);
            if (namePtr) readFn(namePtr, nm, sizeof(nm) - 1);
            if (std::strncmp(nm, "VEngineCvar", 11) == 0 ||
                std::strncmp(nm, "VCvar", 5) == 0)
                return head;
            if (!fallback) fallback = head;
        }
    }
    return fallback;
}

// From InterfaceReg createFn, recover the singleton interface pointer.
uint64_t resolve_create_fn_singleton(uint64_t createFn,
                                     bool (*readFn)(uint64_t, void*, size_t)) noexcept {
    if (!createFn || !readFn) return 0;
    uint8_t code[32]{};
    if (!readFn(createFn, code, sizeof(code))) return 0;

    // Common stubs:
    //   48 8B 05 xx xx xx xx  C3     mov rax,[rip+d]; ret
    //   48 8D 05 xx xx xx xx  C3     lea rax,[rip+d]; ret
    //   48 8B 05 xx xx xx xx  48 85 C0 ... (with null check)
    for (size_t i = 0; i + 7 < sizeof(code); ++i) {
        if (code[i] != 0x48) continue;
        if (code[i + 1] != 0x8B && code[i + 1] != 0x8D) continue;
        if ((code[i + 2] & 0xC7) != 0x05) continue;
        int32_t disp = 0;
        std::memcpy(&disp, code + i + 3, 4);
        const uint64_t rip = createFn + static_cast<uint64_t>(i) + 7;
        const uint64_t target = rip + static_cast<int64_t>(disp);
        if (code[i + 1] == 0x8D) return target;  // static instance address
        uint64_t ptr = 0;
        if (readFn(target, &ptr, 8) && ptr > 0x10000ULL) return ptr;
    }
    return 0;
}

// True CreateInterface → VEngineCvar / ICVar singleton (no module AOB scan).
// Tries tier0 first (canonical), then engine2 if provided via clientBase slot.
uint64_t resolve_icvar_via_create_interface(
    uint64_t moduleBase, size_t moduleSize,
    bool (*readFn)(uint64_t, void*, size_t)) noexcept {
    if (!moduleBase || !moduleSize || !readFn) return 0;

    const uint64_t createFn =
        resolve_remote_export(moduleBase, moduleSize, "CreateInterface", readFn);
    if (!createFn) return 0;

    uint64_t reg = find_interface_reg_head(createFn, readFn);
    if (!reg) return 0;

    // Walk InterfaceReg list; match VEngineCvar* (any version suffix).
    for (int step = 0; step < 512 && reg; ++step) {
        uint64_t namePtr = 0, createImpl = 0, next = 0;
        if (!readFn(reg + 0x08, &namePtr, 8) || !namePtr) break;
        char name[64]{};
        if (!readFn(namePtr, name, sizeof(name) - 1)) break;
        if (std::strncmp(name, "VEngineCvar", 11) == 0 ||
            std::strncmp(name, "VCvar", 5) == 0 ||
            std::strncmp(name, "ICVar", 5) == 0) {
            if (readFn(reg + 0x00, &createImpl, 8) && createImpl) {
                uint64_t iface = resolve_create_fn_singleton(createImpl, readFn);
                if (iface) return iface;
            }
        }
        if (!readFn(reg + 0x10, &next, 8) || !next || next == reg) break;
        reg = next;
    }
    return 0;
}

} // anonymous namespace

bool CvarManager::try_list_path(uint64_t tier0Base, size_t tier0Size,
    uint64_t clientBase, size_t clientSize,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    // Path 2: True ICVar list walk via tier0 CreateInterface (EAT only).
    // No full-module PE/AOB string scan. Falls back to nothing here — caller
    // may try HUD mem / xref paths next.
    //
    // ConVar (Source2 approximate):
    //   +0x00 vtable
    //   +0x08 next*
    //   +0x10 prev*  (sometimes)
    //   +0x18 name*
    //   value float at probed offsets (0x28..0x60)

    if (!readFn) return false;

    // tier0 is the true home of CreateInterface / VEngineCvar. engine2 also
    // exports CreateInterface for other factories — try both (no AOB scan).
    uint64_t icvar = resolve_icvar_via_create_interface(tier0Base, tier0Size, readFn);
    if (!icvar && clientBase && clientSize)
        icvar = resolve_icvar_via_create_interface(clientBase, clientSize, readFn);
    if (!icvar) return false;

    // Live CS2 (2026-07): CCvar holds a ConVar* pointer table at +0x50
    // (dense, ~4k–8k slots). Smaller registration tables at +0x18/+0x30.
    // ConVar name is a char* at +0x00; float value commonly at +0x58
    // (default often +0x70). No linked-list next-walk required.
    const char* targets[] = {
        "hud_scaling", "safezonex", "safezoney",
        "cl_hud_radar_scale", "cl_radar_scale",
        "cl_radar_rotate", "cl_radar_icon_scale_min",
    };

    auto store_value = [&](const char* name, float value) {
        const uint32_t hash = cvar_hash::fnv1a(name);
        if (hash == cvar_hash::hud_scaling) m_values.hudScaling = value;
        else if (hash == cvar_hash::safezonex) m_values.safezoneX = value;
        else if (hash == cvar_hash::safezoney) m_values.safezoneY = value;
        else if (hash == cvar_hash::cl_hud_radar_scale) m_values.clHudRadarScale = value;
        else if (hash == cvar_hash::cl_radar_scale) m_values.clRadarScale = value;
        else if (hash == cvar_hash::cl_radar_rotate) m_values.clRadarRotate = value >= 0.5f;
        else if (hash == cvar_hash::cl_radar_icon_scale_min)
            m_values.clRadarIconScaleMin = value;
    };

    // Prefer current-value float at +0x58 (live dump); fall back to probe_float.
    auto read_cvar_float = [&](uint64_t convar, float fallback) -> float {
        float v = 0.f;
        if (readFn(convar + 0x58, &v, 4) && std::isfinite(v) &&
            v > -100.f && v < 1000.f) {
            // Accept 0 for bools; prefer sensible HUD ranges when set.
            if (v != 0.f || fallback == 0.f) return v;
        }
        // Integer bool stored at +0x58
        int32_t iv = 0;
        if (readFn(convar + 0x58, &iv, 4) && (iv == 0 || iv == 1))
            return static_cast<float>(iv);
        return probe_float(convar, readFn, fallback);
    };

    auto walk_ptr_table = [&](uint64_t arr, uint32_t count) -> int {
        if (!arr || count == 0 || count > 16384) return 0;
        int resolved = 0;
        int needed = static_cast<int>(sizeof(targets) / sizeof(targets[0]));
        for (uint32_t i = 0; i < count && resolved < needed; ++i) {
            uint64_t convar = 0;
            if (!readFn(arr + static_cast<uint64_t>(i) * 8, &convar, 8) ||
                convar < 0x10000)
                continue;
            // Name pointer at +0x00 (live Source2 ConVar slim layout).
            uint64_t namePtr = 0;
            if (!readFn(convar, &namePtr, 8) || namePtr < 0x10000) continue;
            char name[64]{};
            if (!readFn(namePtr, name, sizeof(name) - 1)) continue;
            if (name[0] < 33 || name[0] > 126) continue;

            for (const char* t : targets) {
                if (std::strcmp(name, t) != 0) continue;
                float value = read_cvar_float(convar, -9999.f);
                if (value > -9000.f) {
                    store_value(t, value);
                    ++resolved;
                }
                break;
            }
        }
        return resolved;
    };

    // Table descriptors: (array_offset, count_offset, max_scan).
    // Primary +0x50 is a dense ConVar* table; live build holds client cvars
    // past index 4000, so always scan up to maxScan (null slots are skipped).
    const struct { int arrOff; int cntOff; uint32_t maxScan; bool forceMax; } kTabs[] = {
        {0x50, 0x48, 8192, true},   // primary dense ConVar* table (live)
        {0x30, 0x28, 256, false},   // small registration list
        {0x18, 0x10, 256, false},
    };

    int bestResolved = 0;
    for (const auto& tab : kTabs) {
        uint64_t arr = 0;
        uint32_t count = 0;
        if (!readFn(icvar + static_cast<uint64_t>(tab.arrOff), &arr, 8) || !arr)
            continue;
        if (tab.forceMax) {
            count = tab.maxScan;
        } else {
            readFn(icvar + static_cast<uint64_t>(tab.cntOff), &count, 4);
            if (count == 0 || count > tab.maxScan) count = tab.maxScan;
        }

        const int resolved = walk_ptr_table(arr, count);
        if (resolved > bestResolved) bestResolved = resolved;
        if (resolved >= 3) break;
    }

    // Legacy linked-list fallback (older builds / alternate layouts).
    if (bestResolved < 2) {
        const int kHeadOffs[] = {0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x80};
        for (int headOff : kHeadOffs) {
            uint64_t head = 0;
            if (!readFn(icvar + static_cast<uint64_t>(headOff), &head, 8) || !head)
                continue;
            uint64_t node = head;
            int resolved = 0;
            for (int step = 0; step < 4096 && node; ++step) {
                uint64_t namePtr = 0;
                char name[64]{};
                bool gotName = false;
                for (int no : {0x00, 0x18, 0x10, 0x20}) {
                    if (!readFn(node + static_cast<uint64_t>(no), &namePtr, 8) ||
                        !namePtr)
                        continue;
                    if (!readFn(namePtr, name, sizeof(name) - 1)) continue;
                    if (name[0] < 32 || name[0] > 126) continue;
                    gotName = true;
                    break;
                }
                if (gotName) {
                    for (const char* t : targets) {
                        if (std::strcmp(name, t) == 0) {
                            float value = read_cvar_float(node, -9999.f);
                            if (value > -9000.f) {
                                store_value(t, value);
                                ++resolved;
                            }
                            break;
                        }
                    }
                }
                uint64_t next = 0;
                if (!readFn(node + 0x08, &next, 8)) break;
                if (next == node) break;
                node = next;
            }
            if (resolved > bestResolved) bestResolved = resolved;
            if (resolved >= 3) break;
        }
    }

    if (bestResolved >= 2) {
        m_values.allValid = true;
        return true;
    }
    return false;
}

bool CvarManager::initialize(uint64_t engine2Base, size_t engine2Size,
    uint64_t tier0Base, size_t tier0Size,
    uint64_t clientBase, size_t clientSize,
    uint64_t cHudAddr,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    // Try paths in order: warm cache -> list -> hud -> xref
    if (try_warm_cache_path()) {
        m_values.resolutionPath = 1;
        return true;
    }

    // Path 2: true ICVar via CreateInterface EAT on tier0, then engine2.
    if (try_list_path(tier0Base, tier0Size, clientBase, clientSize, readFn)) {
        m_values.resolutionPath = 2;
        return true;
    }
    if (engine2Base && engine2Size &&
        try_list_path(engine2Base, engine2Size, 0, 0, readFn)) {
        m_values.resolutionPath = 2;
        return true;
    }

    if (try_hud_mem_path(cHudAddr, readFn)) {
        m_values.resolutionPath = 3;
        return true;
    }

    if (try_xref_path(clientBase, clientSize, readFn)) {
        m_values.resolutionPath = 4;
        return true;
    }

    // Set defaults if all paths fail
    m_values.hudScaling = 1.0f;
    m_values.safezoneX = 1.0f;
    m_values.safezoneY = 1.0f;
    m_values.clHudRadarScale = 1.0f;
    m_values.clRadarScale = 0.7f;
    m_values.clRadarRotate = true;
    m_values.clRadarIconScaleMin = 0.4f;
    m_values.allValid = true;
    m_values.resolutionPath = 0;

    return false;
}

template float CvarManager::get<float>(uint32_t hash, float fallback) const noexcept;
template int CvarManager::get<int>(uint32_t hash, int fallback) const noexcept;
template bool CvarManager::get<bool>(uint32_t hash, bool fallback) const noexcept;
template uint32_t CvarManager::get<uint32_t>(uint32_t hash, uint32_t fallback) const noexcept;

} // namespace real::cs2::periscope

