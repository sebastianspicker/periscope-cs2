// hijack_reader.hpp — Handle bridge reader.
//
// Uses handle duplication from a donor process to obtain a reference
// handle to the target process. This avoids directly opening the target
// process with VM_READ access from our own process.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <vector>

namespace real::cs2::hijack {

// ── Undocumented NT structures for handle enumeration ──────────────
struct SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX {
    void*    Object;
    uint64_t UniqueProcessId;
    uint64_t HandleValue;
    uint32_t GrantedAccess;
    uint16_t CreatorBackTraceIndex;
    uint16_t ObjectTypeIndex;
    uint32_t HandleAttributes;
    uint32_t Reserved;
};

struct SYSTEM_HANDLE_INFORMATION_EX {
    uint64_t NumberOfHandles;
    uint64_t Reserved;
    SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX Handles[1];
};

// ── Batch read request ────────────────────────────────────────────
struct BatchRequest {
    uint64_t address{};
    void*    buffer{};
    size_t   size{};
};

// ── Hijack Reader ─────────────────────────────────────────────────
class HijackReader {
public:
    /// Enumerate system handles, find a donor that has a handle to CS2,
    /// duplicate that handle into our process, and cache it.
    ///
    /// @param bootstrapCs2Handle Optional existing handle to CS2 (e.g. from
    ///        educational attach). Used only to learn the kernel Object*
    ///        for precise donor matching — not retained for reads.
    bool setup(uint32_t cs2Pid, void* bootstrapCs2Handle = nullptr) noexcept;

    /// Single read via hijack (re-duplicates handle if closed).
    ///
    /// NOTE: This uses syscall_direct_NtReadVirtualMemory which fires
    /// ETW Threat Intelligence events (V4-V2). For T2+, use the kernel
    /// IOCTL path (IoctlBackend) instead. This path is T0/T1 only.
    bool read(uint64_t address, void* buffer, size_t size) noexcept;

    /// Batch read (up to 16 at once), Fisher-Yates shuffled.
    size_t read_batch(const BatchRequest* requests, size_t count) noexcept;

    /// Shutdown: close all handles.
    void shutdown() noexcept;

    bool     is_ready() const noexcept { return m_ready && m_cs2Handle != 0; }
    uint32_t donor_pid() const noexcept { return m_donorPid; }
    uint32_t cs2_pid()   const noexcept { return m_cs2Pid; }
    uint64_t read_count() const noexcept { return m_readCount; }

    /// Last setup/find failure reason (educational diagnostics).
    const char* last_error() const noexcept { return m_lastError; }
    uint32_t candidates_seen() const noexcept { return m_candidatesSeen; }
    uint32_t preferred_seen() const noexcept { return m_preferredSeen; }
    uint32_t open_fail() const noexcept { return m_openFail; }
    uint32_t dup_fail() const noexcept { return m_dupFail; }
    uint32_t pid_mismatch() const noexcept { return m_pidMismatch; }
    uint32_t object_matched() const noexcept { return m_objectMatched; }

    /// Expose the duplicated CS2 handle for use by backends that need
    /// a raw HANDLE for ReadProcessMemory or similar.
    /// The handle is owned by HijackReader — do NOT close it externally.
    void* cs2_handle() const noexcept { return reinterpret_cast<void*>(m_cs2Handle); }

private:
    void set_error(const char* msg) noexcept { m_lastError = msg ? msg : ""; }
    uint32_t m_cs2Pid{};
    uint32_t m_donorPid{};
    uint64_t m_donorProcHandle{};   // Donor process handle (DUP_HANDLE only)
    uint64_t m_donorHandleValue{};  // Original handle value in donor's handle table
    uint64_t m_cs2Handle{};         // Duplicated CS2 handle (VM_READ)
    uint64_t m_readCount{};
    uint64_t m_nextRotationAt{900};
    uint64_t m_rngState{};
    bool     m_ready{};
    const char* m_lastError{"not started"};
    uint32_t m_candidatesSeen{};
    uint32_t m_preferredSeen{};
    uint32_t m_openFail{};
    uint32_t m_dupFail{};
    uint32_t m_pidMismatch{};
    uint32_t m_objectMatched{};
    void*    m_bootstrapHandle{};  // not owned
    void*    m_cs2Object{};        // kernel object pointer for CS2 process

    /// Phase 1: Enumerate system handles via NtQSI.
    /// Find a CS2 handle owned by a known donor process.
    /// Stores: m_donorPid, m_donorProcHandle, m_donorHandleValue, m_cs2Handle.
    bool find_and_duplicate() noexcept;

    /// Phase 2: Ensure we have a live duplicated handle.
    /// If m_cs2Handle was closed by a previous batch, re-duplicate.
    bool ensure_handle() noexcept;

    /// Close the temporary CS2 handle.
    void close_cs2_handle() noexcept;

    /// Rotate the duplicated CS2 handle: close current, re-duplicate from donor.
    /// Called periodically to prevent handle aging detection.
    bool rotate_handle() noexcept;

};

/// Global instance accessor.
HijackReader& g_Hijack() noexcept;

} // namespace real::cs2::hijack
