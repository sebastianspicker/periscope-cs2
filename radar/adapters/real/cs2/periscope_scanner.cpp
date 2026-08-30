// periscope_scanner.cpp — Periscope-level pattern scanner implementation.

#include "real/cs2/periscope_scanner.hpp"
#include "real/win/xorstr.hpp"

#include <algorithm>
#include <cstring>

namespace real::cs2::periscope {

// ── Encrypted AOB pattern bytes (compile-time XOR'd, zeroed after use) ──
static auto g_enc_c_hud = win::obf::encrypted_pattern<37>(
    {0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,
     0x48, 0x85, 0xC0, 0x0F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x48, 0x89,
     0x5C, 0x24, 0x30, 0x33, 0xDB, 0xF7, 0x80, 0x64, 0x02, 0x00, 0x00,
     0xFF, 0xFF, 0xFF, 0x7F}, build::kXorKeySeed);
static auto g_enc_c_hud_fallback1 = win::obf::encrypted_pattern<19>(
    {0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xD9,
     0x48, 0x85, 0xC0, 0x74, 0x00, 0x48, 0x89, 0x5C, 0x24}, build::kXorKeySeed);
static auto g_enc_c_hud_fallback2 = win::obf::encrypted_pattern<19>(
    {0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xD9,
     0x48, 0x85, 0xC0, 0x0F, 0x84, 0x92, 0x00, 0x00, 0x00}, build::kXorKeySeed);
static auto g_enc_c_hud_panel = win::obf::encrypted_pattern<14>(
    {0x48, 0x89, 0x35, 0x00, 0x00, 0x00, 0x00, 0xE8, 0x00, 0x00,
     0x00, 0x00, 0x48, 0x85}, build::kXorKeySeed);
static auto g_enc_vengine_cvar = win::obf::encrypted_pattern<17>(
    {0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00, 0xC3, 0xCC, 0xCC,
     0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xE9}, build::kXorKeySeed);
static auto g_enc_window_width = win::obf::encrypted_pattern<8>(
    {0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x89, 0x07}, build::kXorKeySeed);
static auto g_enc_window_height = win::obf::encrypted_pattern<8>(
    {0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x89, 0x03}, build::kXorKeySeed);
static auto g_enc_entity_list = win::obf::encrypted_pattern<12>(
    {0x48, 0x89, 0x0D, 0x00, 0x00, 0x00, 0x00, 0xE9, 0x00, 0x00,
     0x00, 0x00}, build::kXorKeySeed);
static auto g_enc_local_controller = win::obf::encrypted_pattern<10>(
    {0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x41, 0x89, 0xBE}, build::kXorKeySeed);
static auto g_enc_local_pawn = win::obf::encrypted_pattern<14>(
    {0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0x48, 0x85, 0xC0,
     0x74, 0x00, 0x8B, 0x88}, build::kXorKeySeed);

// ── Pattern masks (0xFF = match, 0x00 = wildcard; not sensitive, stored in plaintext) ──
static constexpr uint8_t kMask_c_hud[] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF
};
static constexpr uint8_t kMask_c_hud_fallback1[] = {
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0x00,0xFF,0xFF,0xFF,0xFF
};
static constexpr uint8_t kMask_c_hud_fallback2[] = {
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
static constexpr uint8_t kMask_c_hud_panel[] = {
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,
    0x00,0x00,0xFF,0xFF
};
static constexpr uint8_t kMask_vengine_cvar[] = {
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
static constexpr uint8_t kMask_window_width[]  = {0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF};
static constexpr uint8_t kMask_window_height[] = {0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF};
static constexpr uint8_t kMask_entity_list[] = {
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,
    0x00,0x00
};
static constexpr uint8_t kMask_local_controller[] = {0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF};
static constexpr uint8_t kMask_local_pawn[] = {
    0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,
    0xFF,0x00,0xFF,0xFF
};

const std::vector<PatternEntry>& get_patterns() noexcept {
    static const std::vector<PatternEntry> patterns = []() -> std::vector<PatternEntry> {
        std::vector<PatternEntry> result;

#define ADD_PATTERN(name, enc, mask, rip, len)                                          \
    do {                                                                                \
        uint8_t plain[enc.size()];                                                      \
        enc.decrypt(plain, build::kXorKeySeed);                                         \
        std::vector<uint8_t> pv(std::begin(plain), std::end(plain));                    \
        result.push_back({name, pv,                                                     \
            std::vector<uint8_t>(std::begin(mask), std::end(mask)), rip, len});          \
        enc.re_encrypt(plain, build::kXorKeySeed);                                      \
        std::memset(plain, 0, sizeof(plain));                                           \
    } while(0)

        ADD_PATTERN("c_hud",            g_enc_c_hud,            kMask_c_hud,            7, 11);
        ADD_PATTERN("c_hud_fallback1",  g_enc_c_hud_fallback1,  kMask_c_hud_fallback1,  3, 7);
        ADD_PATTERN("c_hud_fallback2",  g_enc_c_hud_fallback2,  kMask_c_hud_fallback2,  3, 7);
        ADD_PATTERN("c_hud_panel",      g_enc_c_hud_panel,      kMask_c_hud_panel,      3, 7);
        ADD_PATTERN("vengine_cvar",     g_enc_vengine_cvar,     kMask_vengine_cvar,     3, 7);
        ADD_PATTERN("window_width",     g_enc_window_width,     kMask_window_width,     2, 6);
        ADD_PATTERN("window_height",    g_enc_window_height,    kMask_window_height,     2, 6);
        ADD_PATTERN("entity_list",      g_enc_entity_list,      kMask_entity_list,      3, 7);
        ADD_PATTERN("local_controller", g_enc_local_controller, kMask_local_controller,  3, 7);
        ADD_PATTERN("local_pawn",       g_enc_local_pawn,       kMask_local_pawn,       3, 7);

#undef ADD_PATTERN
        return result;
    }();
    return patterns;
}

uint64_t resolve_rip(uint64_t matchAddr, int ripOffset,
                     int instructionLen) noexcept {
    // Read signed 32-bit displacement at matchAddr + ripOffset
    int32_t displacement = 0;
    std::memcpy(&displacement,
                reinterpret_cast<const void*>(matchAddr + ripOffset),
                sizeof(displacement));
    return matchAddr + instructionLen + static_cast<int64_t>(displacement);
}

std::vector<ScanResult> scan_all_patterns(
    uint64_t moduleBase,
    size_t moduleSize,
    bool (*readFn)(uint64_t addr, void* buf, size_t size)) noexcept
{
    std::vector<ScanResult> results;
    const auto& patterns = get_patterns();

    for (const auto& pattern : patterns) {
        ScanResult result{pattern.name, 0, false};
        results.push_back(result);
    }

    // Chunked scan: 64KB chunks with pattern size overlap
    constexpr size_t kChunkSize = 0x10000;

    for (size_t pi = 0; pi < patterns.size(); ++pi) {
        const auto& pattern = patterns[pi];
        size_t patternLen = pattern.bytes.size();

        if (patternLen == 0 || patternLen > kChunkSize) continue;

        std::vector<uint8_t> chunk(kChunkSize);

        for (uint64_t offset = 0; offset < moduleSize; offset += kChunkSize) {
            size_t readSize = kChunkSize;
            if (offset + readSize > moduleSize) {
                readSize = moduleSize - offset;
            }

            if (!readFn(moduleBase + offset, chunk.data(), readSize)) {
                continue;
            }

            size_t scanEnd = (readSize > patternLen) ? readSize - patternLen : 0;
            for (size_t i = 0; i <= scanEnd; ++i) {
                bool match = true;
                for (size_t j = 0; j < patternLen; ++j) {
                    if (pattern.mask[j] == 0x00) continue;
                    if (chunk[i + j] != pattern.bytes[j]) {
                        match = false;
                        break;
                    }
                }

                if (match) {
                    uint64_t matchAddr = moduleBase + offset + i;

                    // Resolve RIP-relative if applicable
                    if (pattern.ripOffset >= 0 && pattern.instructionLen > 0) {
                        // We need to read the displacement from the remote memory
                        // Since we already have the chunk data with the offset,
                        // we can compute it directly
                        int32_t displacement = 0;
                        std::memcpy(&displacement,
                                    chunk.data() + i + pattern.ripOffset,
                                    sizeof(displacement));
                        matchAddr = matchAddr + pattern.instructionLen +
                                    static_cast<int64_t>(displacement);
                    }

                    results[pi].address = matchAddr;
                    results[pi].found = true;
                    break;
                }
            }
        }
    }

    return results;
}

} // namespace real::cs2::periscope
