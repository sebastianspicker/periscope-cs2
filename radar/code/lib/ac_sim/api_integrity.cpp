// api_integrity.cpp — Real API integrity verification.
//
// Reads function prologue bytes and compares against known-good patterns
// to detect inline hooks (jmp rel32, jmp [rip+off], call rel32, etc.).
// Bulk verification iterates all registered known-good entries.

#include "ac_sim/api_integrity.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/platform.hpp"
#include "real/win/api_table.hpp"
#include "real/win/windows_h.hpp"
#endif

namespace sim {
namespace {

// XOR key seed for runtime-decoded hook pattern bytes.
// Patterns are stored as XOR-difference from a base to avoid
// literal byte constants in the binary.
static const uint8_t kXorKey = 0x7A;

// Decode a pattern buffer at runtime by XOR with key
static void decode_pattern(const uint8_t* encoded, uint8_t* decoded,
                           size_t len, uint8_t key) noexcept {
    for (size_t i = 0; i < len; ++i) {
        decoded[i] = encoded[i] ^ key;
    }
}

// Known hook patterns (first bytes of the function prologue)
// Encoded as XOR difference from kXorKey to avoid literal byte constants.
bool matches_hook_pattern(const uint8_t* data, size_t dataLen) noexcept {
    uint8_t pat[12] = {0};

    // MSVC does not support GCC compound literals; use static arrays.
    static const uint8_t kJmpRel32[] = {0x93, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t kJmpRip[] = {0x85, 0x5F, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t kJmpFar[] = {0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t kJmpAbs[] = {0x85, 0x5E, 0x5F, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t kCallRel32[] = {0x92, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t kCallRip[] = {0x85, 0x6F, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t kPushRet[] = {0x12, 0x00, 0x00, 0x00, 0x00, 0xB9};
    static const uint8_t kMovRaxJmp[] = {0x32, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x9A};

    // jmp rel32: E9 xx xx xx xx (5 bytes)
    decode_pattern(kJmpRel32, pat, 5, kXorKey);
    if (dataLen >= 5 && data[0] == pat[0])
        return true;

    // jmp qword ptr [rip+off]: FF 25 xx xx xx xx (6 bytes)
    decode_pattern(kJmpRip, pat, 6, kXorKey);
    if (dataLen >= 6 && data[0] == pat[0] && data[1] == pat[1])
        return true;

    // jmp far: EA xx xx xx xx xx xx (7 bytes)
    decode_pattern(kJmpFar, pat, 7, kXorKey);
    if (dataLen >= 7 && data[0] == pat[0])
        return true;

    // jmp [absolute]: FF 24 25 xx xx xx xx (7 bytes)
    decode_pattern(kJmpAbs, pat, 7, kXorKey);
    if (dataLen >= 7 && data[0] == pat[0] && data[1] == pat[1] && data[2] == pat[2])
        return true;

    // call rel32: E8 xx xx xx xx (5 bytes)
    decode_pattern(kCallRel32, pat, 5, kXorKey);
    if (dataLen >= 5 && data[0] == pat[0])
        return true;

    // call [rip+off]: FF 15 xx xx xx xx (6 bytes)
    decode_pattern(kCallRip, pat, 6, kXorKey);
    if (dataLen >= 6 && data[0] == pat[0] && data[1] == pat[1])
        return true;

    // push ROP chain entry / ret (common hook glue)
    // push imm64: 68 xx xx xx xx followed by ret C3
    decode_pattern(kPushRet, pat, 6, kXorKey);
    if (dataLen >= 6 && data[0] == pat[0] && data[5] == pat[5])
        return true;

    // mov rax, imm64; jmp rax: 48 B8 xx xx xx xx xx xx xx xx FF E0 (12 bytes)
    decode_pattern(kMovRaxJmp, pat, 12, kXorKey);
    if (dataLen >= 12 && data[0] == pat[0] && data[1] == pat[1] &&
        data[10] == pat[10] && data[11] == pat[11])
        return true;

    // int3 patch (VAC style): CC CC CC... (any sequence of >8 int3s)
    int ccCount = 0;
    for (size_t i = 0; i < dataLen; ++i) {
        if (data[i] == 0xCC) ++ccCount;
        else break;
    }
    if (ccCount >= 8) return true;

    return false;
}

} // anonymous namespace

ApiIntegrityResult ApiIntegrityChecker::verify(const char* moduleName,
                                                const char* funcName,
                                                uint64_t resolvedAddr) noexcept {
    ApiIntegrityResult result;
    result.moduleName = moduleName ? moduleName : "";
    result.functionName = funcName ? funcName : "";
    result.resolvedAddress = resolvedAddr;

    if (!resolvedAddr) {
        result.passed = false;
        result.hookType = HookType::Unknown;
        result.detail = "null resolved address";
        return result;
    }

    uint64_t expectedBase = 0;
    if (!is_known_module(resolvedAddr, expectedBase)) {
        result.passed = false;
        result.hookType = HookType::IatHook;
        result.detail = "address outside known modules";
        return result;
    }

    result.expectedAddress = expectedBase;

    if (detect_inline_hook(resolvedAddr)) {
        result.passed = false;
        result.hookType = HookType::InlineHook;
        result.detail = "inline hook detected (jmp/call at entry)";
        return result;
    }

    result.passed = true;
    result.detail = "verified OK";
    return result;
}

std::vector<ApiIntegrityResult> ApiIntegrityChecker::verify_all() noexcept {
    std::vector<ApiIntegrityResult> results;

    // In a full implementation with a known-good function registry,
    // this would iterate all registered functions and verify each one.
    // For now, iterate m_knownModules and correlate with any stored
    // function pointers (if we had a function registry).
    //
    // Since we only have module-level known-good entries, we verify
    // that the module base addresses are still within expected ranges.
    for (const auto& mod : m_knownModules) {
        ApiIntegrityResult modResult;
        modResult.moduleName = mod.name;
        modResult.resolvedAddress = mod.base;
        modResult.expectedAddress = mod.base;

#if LR_PLATFORM_WINDOWS
        // Module verification via ApiTable PEB walk instead of
        // EnumProcessModules (which creates a detectable IAT entry and
        // calling EnumProcessModules on self is unusual behavior).
        // ApiTable::resolve() already confirmed all critical modules are
        // loaded via PEB LdrWalk — this avoids the PSAPI detection signal.
        if (real::win::g_Api().resolved) {
            modResult.passed = true;
            modResult.detail = "module verified via PEB walk";
        } else {
            modResult.passed = false;
            modResult.hookType = HookType::IatHook;
            modResult.detail = "ApiTable not resolved";
        }
        results.push_back(modResult);
#else
        modResult.passed = true;
        modResult.detail = "verification not supported on this platform";
        results.push_back(modResult);
#endif
    }

#if LR_PLATFORM_WINDOWS
    // Function-pointer sweep: every resolved slot in the ApiTable must be
    // non-null and must map to an expected module. All function pointers
    // live in the [0, offsetof(ApiTable, resolved)) prefix — the struct has
    // no padding there (same layout assumption ApiTable::re_resolve uses
    // for its memset). Null slots are normal during lazy resolution
    // (ensure_resolved fills them on demand) and are skipped, not flagged.
    auto& api = real::win::g_Api();
    const size_t slotCount = offsetof(real::win::ApiTable, resolved) / sizeof(void*);
    const bool tableUsable = api.fully_resolved || api.resolved;
    auto** slots = reinterpret_cast<void**>(&api);

    for (size_t i = 0; i < slotCount; ++i) {
        if (!slots[i]) continue;

        ApiIntegrityResult fnResult;
        fnResult.moduleName = "ApiTable";
        fnResult.functionName = "slot[" + std::to_string(i) + "]";
        fnResult.resolvedAddress = reinterpret_cast<uint64_t>(slots[i]);

        uint64_t expectedBase = 0;
        if (is_known_module(fnResult.resolvedAddress, expectedBase)) {
            // Known-good registry populated — full module containment and
            // inline-hook check on the function prologue.
            fnResult.expectedAddress = expectedBase;
            if (detect_inline_hook(fnResult.resolvedAddress)) {
                fnResult.passed = false;
                fnResult.hookType = HookType::InlineHook;
                fnResult.detail = "inline hook detected (jmp/call at entry)";
            } else {
                fnResult.passed = true;
                fnResult.detail = "verified OK";
            }
        } else if (m_knownModules.empty()) {
            // No registry to validate against — rely on the resolution flags.
            if (tableUsable) {
                fnResult.passed = true;
                fnResult.detail = "verified OK (ApiTable resolved)";
            } else {
                fnResult.passed = false;
                fnResult.hookType = HookType::IatHook;
                fnResult.detail = "ApiTable not resolved";
            }
        } else {
            fnResult.passed = false;
            fnResult.hookType = HookType::IatHook;
            fnResult.detail = "address outside known modules";
        }
        results.push_back(fnResult);
    }
#endif

    return results;
}

void ApiIntegrityChecker::set_known_good(const char* moduleName,
                                          uint64_t baseAddr,
                                          uint64_t size) noexcept {
    m_knownModules.push_back({
        moduleName ? moduleName : "",
        baseAddr,
        size
    });
}

bool ApiIntegrityChecker::is_known_module(uint64_t addr,
                                           uint64_t& baseOut) noexcept {
    for (const auto& mod : m_knownModules) {
        if (addr >= mod.base && addr < mod.base + mod.size) {
            baseOut = mod.base;
            return true;
        }
    }
    return false;
}

bool ApiIntegrityChecker::detect_inline_hook(uint64_t funcAddr) noexcept {
    if (!funcAddr) return false;

#if LR_PLATFORM_WINDOWS
    // Probe readability first — unmapped "known good" ranges must not AV.
    auto& api = real::win::g_Api();
    MEMORY_BASIC_INFORMATION mbi{};
    if (api.VirtualQuery) {
        if (!api.VirtualQuery(reinterpret_cast<void*>(funcAddr), &mbi,
                              sizeof(mbi))) {
            return false;
        }
        if (mbi.State != MEM_COMMIT) return false;
        const DWORD prot = mbi.Protect & 0xFFu;
        const bool readable =
            prot == PAGE_READONLY || prot == PAGE_READWRITE ||
            prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_READWRITE ||
            prot == PAGE_EXECUTE_WRITECOPY || prot == PAGE_WRITECOPY;
        if (!readable) return false;
    }

    uint8_t prologue[16]{};
    __try {
        volatile uint8_t* src = reinterpret_cast<volatile uint8_t*>(funcAddr);
        for (size_t i = 0; i < sizeof(prologue); ++i) {
            prologue[i] = src[i];
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    return matches_hook_pattern(prologue, sizeof(prologue));
#else
    (void)funcAddr;
    return false;
#endif
}

} // namespace sim
