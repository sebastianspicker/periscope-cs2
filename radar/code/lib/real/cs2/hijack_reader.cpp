// Split from hijack_reader.cpp — see MONOLITH_REFACTOR_LEDGER.
#include "real/cs2/hijack_reader.hpp"
#include "real/cs2/hijack_reader_internal.hpp"
#include "real/cs2/process_cache.hpp"
#include "real/win/api_table.hpp"
#include "real/win/syscall_helper.hpp"
#include "real/win/timing.hpp"
#include "real/win/xorstr.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace real::cs2::hijack {

bool HijackReader::ensure_handle() noexcept {
#if LR_PLATFORM_WINDOWS
    if (m_cs2Handle) return true;

    auto& api = real::win::g_Api();
    if (!api.resolved) return false;

    // If we have a donor handle and the donor's original handle value,
    // re-duplicate without re-enumerating (avoids redundant NtQSI calls)
    if (m_donorProcHandle && m_donorHandleValue) {
        HANDLE donorProc = reinterpret_cast<HANDLE>(m_donorProcHandle);
        HANDLE dupe = nullptr;

        NTSTATUS dupResult = real::win::syscall_direct_NtDuplicateObject(
            donorProc,
            reinterpret_cast<HANDLE>(m_donorHandleValue),
            ::GetCurrentProcess(),
            &dupe,
            PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
            0,
            0);

        if (NT_SUCCESS(dupResult) && dupe) {
            // Verify PID
            DWORD dupePid = api.GetProcessId(dupe);
            if (dupePid == m_cs2Pid) {
                m_cs2Handle = reinterpret_cast<uint64_t>(dupe);
                return true;
            }
            api.CloseHandle(dupe);
        }

        // Re-duplication failed — fall through (find_and_duplicate hits the
        // cache counter check and returns false without re-enumerating)
    }

    // Static re-entrancy guard: prevent repeated NtQSI re-enumeration
    // after find_and_duplicate() has already failed in a prior call.
    static bool s_reentering = false;
    if (s_reentering) return false;

    s_reentering = true;

    // Clean up old donor state before re-enumeration
    if (m_donorProcHandle) {
        api.CloseHandle(reinterpret_cast<HANDLE>(m_donorProcHandle));
        m_donorProcHandle = 0;
    }
    m_donorHandleValue = 0;

    bool ok = find_and_duplicate();
    if (ok) {
        s_reentering = false;
    }
    return ok;
#else
    return false;
#endif
}

// ═══════════════════════════════════════════════════════════════════════
// setup: find donor + duplicate handle in one step.
// ═══════════════════════════════════════════════════════════════════════

bool HijackReader::setup(uint32_t cs2Pid, void* bootstrapCs2Handle) noexcept {
    m_cs2Pid = cs2Pid;
    m_bootstrapHandle = bootstrapCs2Handle;
    m_ready = false;
    m_objectMatched = 0;
    m_cs2Object = nullptr;
    // Allow a fresh NtQSI on each explicit setup() (demos call once at start).
    // ensure_handle still avoids re-enum when cache is warm.
    if (!g_cachedDonor.valid) g_ntqsi40_call_count = 0;
    if (!find_and_duplicate()) {
        // One more attempt with force re-enum if cache was stale.
        g_ntqsi40_call_count = 0;
        g_cachedDonor = {};
        if (!find_and_duplicate()) return false;
    }
    m_rngState = static_cast<uint64_t>(cs2Pid) ^ 0x9E3779B97F4A7C15ULL;
    m_nextRotationAt = 900 + (xor_shift(m_rngState) % 601);
    m_ready = true;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// read: single memory read via duplicated handle.
// ═══════════════════════════════════════════════════════════════════════

bool HijackReader::read(uint64_t address, void* buffer, size_t size) noexcept {
#if LR_PLATFORM_WINDOWS
    if (!m_ready || !buffer || size == 0) return false;
    if (!ensure_handle()) return false;

    auto& api = real::win::g_Api();
    if (!api.resolved) return false;

    HANDLE hCs2 = reinterpret_cast<HANDLE>(m_cs2Handle);
    SIZE_T bytesRead = 0;

    // Use direct syscall instead of API table NtReadVirtualMemory
    // to avoid ntdll call-table hooks (VAC hooks ntdll's exported entry point)
    NTSTATUS status = real::win::syscall_direct_NtReadVirtualMemory(
        hCs2,
        reinterpret_cast<PVOID>(address),
        buffer,
        size,
        &bytesRead);

    if (status < 0) {
        // Handle may be stale — close and re-acquire
        close_cs2_handle();
        return false;
    }

    ++m_readCount;
    if (m_readCount >= m_nextRotationAt) {
        m_nextRotationAt = m_readCount + 900 + (xor_shift(m_rngState) % 601);
        rotate_handle();
    }
    return bytesRead == size;
#else
    (void)address; (void)buffer; (void)size;
    return false;
#endif
}

// ═══════════════════════════════════════════════════════════════════════
// read_batch: batch read with Fisher-Yates shuffle.
// ═══════════════════════════════════════════════════════════════════════

size_t HijackReader::read_batch(const BatchRequest* requests, size_t count) noexcept {
    if (!m_ready || !requests || count == 0 || count > 16) return 0;

    // Rotate handle if threshold reached (before batch to ensure fresh handle)
    if (m_readCount >= m_nextRotationAt) {
        m_nextRotationAt = m_readCount + 900 + (xor_shift(m_rngState) % 601);
        rotate_handle();
    }
    if (!ensure_handle()) return 0;

    // Copy and Fisher-Yates shuffle
    BatchRequest shuffled[16];
    size_t actual = std::min(count, static_cast<size_t>(16));
    std::memcpy(shuffled, requests, actual * sizeof(BatchRequest));

    // Use a simple XOR-Shift for jitter to avoid heavy RNG construction
    uint64_t rng = m_readCount * 0x9E3779B97F4A7C15ULL;
    for (size_t i = actual; i > 1; --i) {
        rng = rng * 0x5851F42D4C957F2DULL + 0x14057B7EF767814FULL;
        size_t j = static_cast<size_t>(rng % i);
        std::swap(shuffled[i - 1], shuffled[j]);
    }

    size_t successCount = 0;
    for (size_t i = 0; i < actual; ++i) {
        if (read(shuffled[i].address, shuffled[i].buffer, shuffled[i].size)) {
            ++successCount;
        }
    }

    return successCount;
}

// ═══════════════════════════════════════════════════════════════════════
// rotate_handle: periodic handle rotation.
// ═══════════════════════════════════════════════════════════════════════

bool HijackReader::rotate_handle() noexcept {
    close_cs2_handle();
    if (m_donorProcHandle && m_donorHandleValue) {
        return ensure_handle();
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════
// close_cs2_handle / shutdown
// ═══════════════════════════════════════════════════════════════════════

void HijackReader::close_cs2_handle() noexcept {
#if LR_PLATFORM_WINDOWS
    if (m_cs2Handle) {
        auto& api = real::win::g_Api();
        if (api.resolved && api.CloseHandle) {
            api.CloseHandle(reinterpret_cast<HANDLE>(m_cs2Handle));
        }
        m_cs2Handle = 0;
        // Intentionally does NOT reset m_donorHandleValue or m_donorProcHandle
        // so ensure_handle() can re-duplicate without re-enumerating.
    }
#endif
}

void HijackReader::shutdown() noexcept {
    close_cs2_handle();
#if LR_PLATFORM_WINDOWS
    if (m_donorProcHandle) {
        auto& api = real::win::g_Api();
        if (api.resolved && api.CloseHandle) {
            api.CloseHandle(reinterpret_cast<HANDLE>(m_donorProcHandle));
        }
        m_donorProcHandle = 0;
    }
#endif
    m_ready = false;
    m_readCount = 0;
    m_nextRotationAt = 900;
    m_rngState = 0;
    m_donorPid = 0;
    m_donorHandleValue = 0;
}

HijackReader& g_Hijack() noexcept {
    static HijackReader reader;
    return reader;
}

} // namespace real::cs2::hijack

