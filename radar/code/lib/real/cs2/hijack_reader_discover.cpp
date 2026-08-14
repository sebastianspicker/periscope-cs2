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

namespace detail {

CachedDonorInfo g_cachedDonor{};
int g_ntqsi40_call_count = 0;

// Preferred donor basenames — discovered via process list (Toolhelp/NtQSI
// cache), NOT per-handle OpenProcess+QueryFullProcessImageName. That keeps
// donor discovery handle-free for naming and avoids open_fail storms.
bool is_preferred_donor_basename(const char* base) noexcept {
    if (!base || !base[0]) return false;
    static const char* kPref[] = {
        "steam.exe", "steamwebhelper.exe", "gameoverlayui64.exe",
        "gameoverlayui.exe", "steamservice.exe",
        "discord.exe", "discordptb.exe", "discordcanary.exe",
        "chrome.exe", "msedge.exe", "firefox.exe",
        "explorer.exe", "SearchHost.exe", "RuntimeBroker.exe",
        "nvcontainer.exe", "obs64.exe", "obs.exe",
        "EpicGamesLauncher.exe", "Origin.exe", "EADesktop.exe",
    };
    for (const char* pref : kPref) {
        if (_stricmp(base, pref) == 0) return true;
    }
    // Substring match for Steam family (steamwebhelper, etc.)
    if (std::strstr(base, "steam") || std::strstr(base, "Steam")) return true;
    if (std::strstr(base, "discord") || std::strstr(base, "Discord")) return true;
    return false;
}

// Build preferred donor PID set from process cache / Toolhelp — no OpenProcess.
std::vector<uint32_t> collect_preferred_donor_pids(uint32_t cs2Pid) noexcept {
    std::vector<uint32_t> pids;
    pids.reserve(32);

    // Primary: process cache (NtQSI SystemProcessInformation) already has names.
    const auto& list = real::cs2::get_cached_process_list();
    for (const auto& e : list) {
        if (e.pid == 0 || e.pid == cs2Pid) continue;
        if (is_preferred_donor_basename(e.name.c_str())) {
            pids.push_back(e.pid);
        }
    }

    // Secondary: Toolhelp snapshot (stable, no OpenProcess).
    auto& api = real::win::g_Api();
    if (api.resolved && api.CreateToolhelp32Snapshot && api.Process32FirstW &&
        api.Process32NextW) {
        HANDLE snap = api.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap && snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe{};
            pe.dwSize = sizeof(pe);
            if (api.Process32FirstW(snap, &pe)) {
                do {
                    if (pe.th32ProcessID == 0 || pe.th32ProcessID == cs2Pid)
                        continue;
                    char name[MAX_PATH]{};
                    WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, name,
                                        static_cast<int>(sizeof(name)), nullptr,
                                        nullptr);
                    if (!is_preferred_donor_basename(name)) continue;
                    const uint32_t pid = pe.th32ProcessID;
                    if (std::find(pids.begin(), pids.end(), pid) == pids.end())
                        pids.push_back(pid);
                } while (api.Process32NextW(snap, &pe));
            }
            if (api.CloseHandle) api.CloseHandle(snap);
            else ::CloseHandle(snap);
        }
    }
    return pids;
}

bool pid_in_set(const std::vector<uint32_t>& pids, uint32_t pid) noexcept {
    return std::find(pids.begin(), pids.end(), pid) != pids.end();
}

// Open a donor for handle duplication. Prefer DUP_HANDLE only; fall back to
// DUP|QUERY. Does NOT open CS2 — only the donor process (handle-free CS2 path).
HANDLE open_donor_for_dup(uint32_t donorPid) noexcept {
    auto& api = real::win::g_Api();
    if (!api.resolved || !api.OpenProcess) return nullptr;

    // 1) Minimal rights
    HANDLE h = api.OpenProcess(PROCESS_DUP_HANDLE, FALSE, donorPid);
    if (h) return h;

    // 2) DUP + limited query (some Windows builds want both)
    h = api.OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION,
                        FALSE, donorPid);
    if (h) return h;

    // 3) NtOpenProcess with CLIENT_ID (bypasses some OpenProcess filters)
    if (api.NtOpenProcess) {
        HANDLE ntH = nullptr;
        OBJECT_ATTRIBUTES oa{};
        oa.Length = sizeof(oa);
        CLIENT_ID cid{};
        cid.UniqueProcess = reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(donorPid));
        cid.UniqueThread = nullptr;
        NTSTATUS st = api.NtOpenProcess(
            &ntH, PROCESS_DUP_HANDLE, &oa, &cid);
        if (st >= 0 && ntH) return ntH;
        st = api.NtOpenProcess(
            &ntH, PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION, &oa,
            &cid);
        if (st >= 0 && ntH) return ntH;
    }
    return nullptr;
}

// True only if we can RPM a real PE MZ via PEB→ImageBase (needs QUERY+VM_READ).
bool handle_has_vm_read(HANDLE h) noexcept {
    if (!h) return false;
    auto& api = real::win::g_Api();
    if (!api.resolved || !api.NtQueryInformationProcess) return false;

    struct {
        PVOID Reserved1;
        PVOID PebBaseAddress;
        PVOID Reserved2[2];
        ULONG_PTR UniqueProcessId;
        PVOID Reserved3;
    } pbi{};
    ULONG retLen = 0;
    if (api.NtQueryInformationProcess(h, 0, &pbi, sizeof(pbi), &retLen) < 0 ||
        !pbi.PebBaseAddress)
        return false;

    // PEB.ImageBaseAddress @ +0x10 on x64
    uint64_t imageBase = 0;
    SIZE_T br = 0;
    NTSTATUS rs = real::win::syscall_direct_NtReadVirtualMemory(
        h, reinterpret_cast<PVOID>(reinterpret_cast<uint64_t>(pbi.PebBaseAddress) + 0x10),
        &imageBase, sizeof(imageBase), &br);
    if (rs < 0 || br != sizeof(imageBase) || imageBase < 0x10000) {
        if (api.NtReadVirtualMemory) {
            br = 0;
            rs = api.NtReadVirtualMemory(
                h,
                reinterpret_cast<PVOID>(reinterpret_cast<uint64_t>(pbi.PebBaseAddress) +
                                        0x10),
                &imageBase, sizeof(imageBase), &br);
        }
        if (rs < 0 || br != sizeof(imageBase) || imageBase < 0x10000) return false;
    }

    uint16_t mz = 0;
    br = 0;
    rs = real::win::syscall_direct_NtReadVirtualMemory(
        h, reinterpret_cast<PVOID>(imageBase), &mz, sizeof(mz), &br);
    if (rs < 0 || br != sizeof(mz) || mz != 0x5A4D) {
        if (api.NtReadVirtualMemory) {
            br = 0;
            rs = api.NtReadVirtualMemory(h, reinterpret_cast<PVOID>(imageBase), &mz,
                                         sizeof(mz), &br);
        }
        if (rs < 0 || br != sizeof(mz) || mz != 0x5A4D) return false;
    }
    return true;
}

NTSTATUS duplicate_cs2_handle(HANDLE donorProc, HANDLE sourceHandle, HANDLE* outDupe) noexcept {
    *outDupe = nullptr;
    auto& api = real::win::g_Api();

    auto try_keep = [&](HANDLE dupe) -> bool {
        if (!dupe) return false;
        if (!handle_has_vm_read(dupe)) {
            if (api.resolved && api.CloseHandle) api.CloseHandle(dupe);
            else ::CloseHandle(dupe);
            return false;
        }
        *outDupe = dupe;
        return true;
    };

    // Prefer explicit VM_READ — SAME_ACCESS first often yields QUERY-only
    // Steam handles that pass GetProcessId but cannot RPM.
    NTSTATUS st = real::win::syscall_direct_NtDuplicateObject(
        donorProc, sourceHandle, ::GetCurrentProcess(), outDupe,
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, 0, 0);
    if (st >= 0 && try_keep(*outDupe)) return st;
    *outDupe = nullptr;

    st = real::win::syscall_direct_NtDuplicateObject(
        donorProc, sourceHandle, ::GetCurrentProcess(), outDupe,
        PROCESS_VM_READ, 0, 0);
    if (st >= 0 && try_keep(*outDupe)) return st;
    *outDupe = nullptr;

    // SAME_ACCESS only if donor already has VM_READ in the mask.
    st = real::win::syscall_direct_NtDuplicateObject(
        donorProc, sourceHandle, ::GetCurrentProcess(), outDupe,
        0, 0, 0x2 /* DUPLICATE_SAME_ACCESS */);
    if (st >= 0 && try_keep(*outDupe)) return st;
    *outDupe = nullptr;

    if (api.resolved && api.NtDuplicateObject) {
        st = api.NtDuplicateObject(donorProc, sourceHandle, ::GetCurrentProcess(),
                                   outDupe, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
                                   0, 0);
        if (st >= 0 && try_keep(*outDupe)) return st;
        *outDupe = nullptr;
        st = api.NtDuplicateObject(donorProc, sourceHandle, ::GetCurrentProcess(),
                                   outDupe, 0, 0, 0x2);
        if (st >= 0 && try_keep(*outDupe)) return st;
        *outDupe = nullptr;
    }

    HANDLE dupe = nullptr;
    if (DuplicateHandle(donorProc, sourceHandle, ::GetCurrentProcess(), &dupe,
                        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, 0) &&
        try_keep(dupe)) {
        return 0;
    }
    dupe = nullptr;
    if (DuplicateHandle(donorProc, sourceHandle, ::GetCurrentProcess(), &dupe,
                        PROCESS_VM_READ, FALSE, 0) &&
        try_keep(dupe)) {
        return 0;
    }
    dupe = nullptr;
    if (DuplicateHandle(donorProc, sourceHandle, ::GetCurrentProcess(), &dupe,
                        0, FALSE, DUPLICATE_SAME_ACCESS) &&
        try_keep(dupe)) {
        return 0;
    }
    return st < 0 ? st : static_cast<NTSTATUS>(0xC0000001L);
}

} // namespace detail

#if LR_PLATFORM_WINDOWS
using detail::open_donor_for_dup;
using detail::duplicate_cs2_handle;
using detail::collect_preferred_donor_pids;
using detail::pid_in_set;
#endif

// ═══════════════════════════════════════════════════════════════════════
// find_and_duplicate: the core operation.
// 1. Enumerate all system handles via NtQSI (ExtendedHandleInformation 0x40)
// 2. For each handle pointing to CS2 PID, check if owner is a known donor
// 3. Open the donor with PROCESS_DUP_HANDLE
// 4. Duplicate the CS2 handle from donor into our process
// 5. Verify the duplicated handle (GetProcessId matches CS2)
// 6. Cache both the donor handle and the duplicated CS2 handle
// ═══════════════════════════════════════════════════════════════════════

bool HijackReader::find_and_duplicate() noexcept {
#if LR_PLATFORM_WINDOWS
    m_candidatesSeen = m_preferredSeen = m_openFail = m_dupFail = m_pidMismatch = 0;
    m_objectMatched = 0;

    // Best-effort SeDebugPrivilege so NtQSI returns Object* (else often NULL).
    {
        auto& api = real::win::g_Api();
        if (api.resolved && api.RtlAdjustPrivilege) {
            BOOL was = FALSE;
            // SE_DEBUG_PRIVILEGE = 20
            api.RtlAdjustPrivilege(20, TRUE, FALSE, &was);
        }
    }

    // Enforce max 1 call to NtQSI(0x40) — cached donor info is used on retry
    if (g_ntqsi40_call_count >= 1) {
        if (g_cachedDonor.valid) {
            m_donorPid = g_cachedDonor.donorPid;
            m_donorHandleValue = g_cachedDonor.donorHandleValue;
            auto& api = real::win::g_Api();
            if (!api.resolved) {
                set_error("api table not resolved (cache path)");
                return false;
            }
            HANDLE donorProc = open_donor_for_dup(m_donorPid);
            if (!donorProc) {
                set_error("OpenProcess(DUP_HANDLE) on cached donor failed (rights?)");
                ++m_openFail;
                return false;
            }
            HANDLE dupe = nullptr;
            NTSTATUS dupResult = duplicate_cs2_handle(
                donorProc, reinterpret_cast<HANDLE>(m_donorHandleValue), &dupe);
            if (dupResult < 0 || !dupe) {
                api.CloseHandle(donorProc);
                set_error("duplicate from cached donor failed");
                ++m_dupFail;
                return false;
            }
            DWORD dupePid = api.GetProcessId ? api.GetProcessId(dupe) : 0;
            if (dupePid == 0) dupePid = ::GetProcessId(dupe);
            if (dupePid == m_cs2Pid) {
                m_donorProcHandle = reinterpret_cast<uint64_t>(donorProc);
                m_cs2Handle = reinterpret_cast<uint64_t>(dupe);
                m_donorPid = g_cachedDonor.donorPid;
                set_error("ok");
                return true;
            }
            api.CloseHandle(dupe);
            api.CloseHandle(donorProc);
            set_error("cached donor dupe pid mismatch");
            ++m_pidMismatch;
            return false;
        }
        set_error("NtQSI already used and no valid donor cache");
        return false;
    }

    // Reset all cached state before attempting re-enumeration.
    // On success these are overwritten by try_duplicate_from_donor;
    // on failure they remain zeroed so stale handles are never used.
    m_donorPid = 0;
    m_donorProcHandle = 0;
    m_donorHandleValue = 0;
    m_cs2Handle = 0;

    auto& api = real::win::g_Api();
    if (!api.resolved) {
        set_error("api table not resolved");
        return false;
    }

    // Quick PID existence check via NtOpenProcess instead of a full
    // NtQSI(SystemProcessInformation) enumeration. The cached process
    // list is used by other call sites that need full enumeration.
    (void)pid_exists(m_cs2Pid);

    // NtQSI(SystemExtendedHandleInformation, info_class=0x40) is the #1
    // VAC-monitored info class. Each call is a high-risk detection event.
    // We minimize to ONE call on setup; re-duplication reuses cached handles
    // without re-enumerating (see ensure_handle).
    //
    // Use NtQSI with ExtendedHandleInformation (0x40) only.
    // Windows 8+ supports extended handles exclusively — legacy 0x10
    // path removed to avoid detectable API call pattern.
    ++g_ntqsi40_call_count;
    // Handle table can be tens of MB on busy systems — grow until STATUS_SUCCESS.
    ULONG bufSize = 1u << 20;  // 1 MB start
    std::vector<uint8_t> buffer(bufSize);

    real::win::yield();
    NTSTATUS status = 0xC0000004L;  // STATUS_INFO_LENGTH_MISMATCH
    for (int grow = 0; grow < 8; ++grow) {
        status = api.NtQuerySystemInformation(0x40, buffer.data(), bufSize, &bufSize);
        if (status >= 0) break;
        if (bufSize <= buffer.size()) bufSize = static_cast<ULONG>(buffer.size() * 2);
        buffer.resize(bufSize);
    }
    real::win::yield();

    if (status < 0) {
        set_error("NtQuerySystemInformation(0x40) failed (buffer/rights)");
        return false;
    }

    // Preferred donors by name (Toolhelp/process cache — no OpenProcess).
    const std::vector<uint32_t> preferredPids =
        collect_preferred_donor_pids(m_cs2Pid);
    m_preferredSeen = static_cast<uint32_t>(preferredPids.size());

    // Extended handle format (64-bit handles)
    auto* info = reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(
        buffer.data());
    uint64_t numHandles = info->NumberOfHandles;

    // Learn CS2 kernel Object* + Process ObjectTypeIndex from bootstrap handle.
    m_cs2Object = nullptr;
    uint16_t processTypeIndex = 0;
    if (m_bootstrapHandle) {
        const DWORD selfPid = api.GetCurrentProcessId
                                  ? api.GetCurrentProcessId()
                                  : ::GetCurrentProcessId();
        const uint64_t bootVal =
            static_cast<uint64_t>(reinterpret_cast<uintptr_t>(m_bootstrapHandle));
        for (uint64_t i = 0; i < numHandles; ++i) {
            auto& h = info->Handles[i];
            if (static_cast<uint32_t>(h.UniqueProcessId) != selfPid) continue;
            if (h.HandleValue != bootVal &&
                (h.HandleValue & 0xFFFFFFFFull) != (bootVal & 0xFFFFFFFFull))
                continue;
            m_cs2Object = h.Object;  // may be NULL without SeDebug
            processTypeIndex = h.ObjectTypeIndex;
            break;
        }
    }

    // Helper: open a donor process and try to duplicate a handle value
    auto try_duplicate_from_donor = [&](uint32_t donorPid,
                                         uint64_t handleValue) -> bool {
        // Open DONOR only (never CS2) — handle-free CS2 OpenProcess path.
        HANDLE donorProc = open_donor_for_dup(donorPid);
        if (!donorProc) {
            ++m_openFail;
            return false;
        }

        HANDLE dupe = nullptr;
        NTSTATUS dupResult = duplicate_cs2_handle(
            donorProc, reinterpret_cast<HANDLE>(handleValue), &dupe);

        if (dupResult < 0 || !dupe) {
            api.CloseHandle(donorProc);
            ++m_dupFail;
            return false;
        }

        // Verify: the duplicated handle should point to our CS2 PID.
        DWORD dupePid = api.GetProcessId ? api.GetProcessId(dupe) : 0;
        if (dupePid == 0) dupePid = ::GetProcessId(dupe);
        if (dupePid == 0 && api.NtQueryInformationProcess) {
            struct {
                PVOID Reserved1;
                PVOID PebBaseAddress;
                PVOID Reserved2[2];
                ULONG_PTR UniqueProcessId;
                PVOID Reserved3;
            } pbi{};
            ULONG retLen = 0;
            if (api.NtQueryInformationProcess(dupe, 0, &pbi, sizeof(pbi), &retLen) >= 0)
                dupePid = static_cast<DWORD>(pbi.UniqueProcessId);
        }
        if (dupePid == 0 || dupePid != m_cs2Pid) {
            api.CloseHandle(dupe);
            api.CloseHandle(donorProc);
            ++m_pidMismatch;
            return false;
        }

        // handle_has_vm_read already enforced inside duplicate_cs2_handle.
        m_donorProcHandle = reinterpret_cast<uint64_t>(donorProc);
        m_donorHandleValue = handleValue;
        m_cs2Handle = reinterpret_cast<uint64_t>(dupe);
        m_donorPid = donorPid;
        return true;
    };

    struct DonorCandidate {
        uint32_t pid;
        uint64_t handleValue;
        uint32_t access;
        bool preferred;
        bool has_vm_read;
        bool object_match;
    };
    std::vector<DonorCandidate> objectHits;  // exact CS2 Object*
    std::vector<DonorCandidate> preferred;
    std::vector<DonorCandidate> others;
    objectHits.reserve(64);
    preferred.reserve(256);
    others.reserve(512);

    for (uint64_t i = 0; i < numHandles; ++i) {
        auto& h = info->Handles[i];
        uint64_t ownerPid = h.UniqueProcessId;
        if (ownerPid == 0 || ownerPid == m_cs2Pid) continue;

        const bool objMatch =
            m_cs2Object != nullptr && h.Object != nullptr && h.Object == m_cs2Object;
        if (objMatch) ++m_objectMatched;

        // Type filter: only Process objects when we know the type index.
        if (processTypeIndex != 0 && h.ObjectTypeIndex != processTypeIndex)
            continue;

        // With Object* known: only exact CS2 process objects.
        if (m_cs2Object != nullptr && !objMatch) continue;

        const bool isPref = pid_in_set(preferredPids, static_cast<uint32_t>(ownerPid));
        const uint32_t access = h.GrantedAccess;
        const bool has_vm = (access & PROCESS_VM_READ) != 0 || (access & 0x0010) != 0;
        const bool can_read =
            has_vm || (access & 0x1FFFFF) == 0x1FFFFF ||
            (access & 0x0FFF) == 0x0FFF || (access & 0x1F0FFF) == 0x1F0FFF ||
            (access & PROCESS_QUERY_INFORMATION) != 0 ||
            (access & PROCESS_QUERY_LIMITED_INFORMATION) != 0 ||
            (access & 0x1000) != 0;

        // Preferred donors: any Process handle (verified via GetProcessId after dupe).
        // Non-preferred: only if access looks like VM_READ / full process rights.
        if (!objMatch) {
            if (isPref) {
                if (access == 0) continue;
            } else {
                if (!can_read) continue;
            }
        } else if (access == 0) {
            continue;
        }

        DonorCandidate c{};
        c.pid = static_cast<uint32_t>(ownerPid);
        c.handleValue = h.HandleValue;
        c.access = access;
        c.preferred = isPref;
        c.has_vm_read = has_vm;
        c.object_match = objMatch;
        if (objMatch) {
            objectHits.push_back(c);
        } else if (c.preferred) {
            preferred.push_back(c);
        } else if (others.size() < 256) {
            others.push_back(c);
        }
        // Cap preferred Process handles high — verification is the filter.
        if (objectHits.size() + preferred.size() >= 4096) break;
    }
    m_candidatesSeen = static_cast<uint32_t>(
        objectHits.size() + preferred.size() + others.size());

    if (objectHits.empty() && preferred.empty() && others.empty()) {
        set_error(m_cs2Object
                      ? "zero system handles reference CS2 Object (no donor)"
                      : "zero handle candidates on system (no donor process)");
        return false;
    }

    // Order: object-matched + VM_READ first, then preferred, then others.
    auto by_vm = [](const DonorCandidate& c) { return c.has_vm_read; };
    auto by_pref = [](const DonorCandidate& c) { return c.preferred; };
    std::stable_partition(objectHits.begin(), objectHits.end(), by_vm);
    std::stable_partition(objectHits.begin(), objectHits.end(), by_pref);
    std::stable_partition(preferred.begin(), preferred.end(), by_vm);
    std::stable_partition(others.begin(), others.end(), by_vm);

    std::vector<DonorCandidate> ordered;
    ordered.reserve(objectHits.size() + preferred.size() + others.size());
    ordered.insert(ordered.end(), objectHits.begin(), objectHits.end());
    ordered.insert(ordered.end(), preferred.begin(), preferred.end());
    ordered.insert(ordered.end(), others.begin(), others.end());

    // Randomize start among preferred block to avoid sticky donor PID.
    uint64_t rng = static_cast<uint64_t>(m_cs2Pid) ^
                   (m_readCount * 0x9E3779B97F4A7C15ULL) ^
                   build::kXorKeySeed;
    rng = rng * 0x5851F42D4C957F2DULL + 0x14057B7EF767814FULL;
    const size_t nPref = preferred.size();
    const size_t n = ordered.size();
    const size_t startIdx =
        nPref > 0 ? static_cast<size_t>(rng % nPref) : static_cast<size_t>(rng % n);

    for (size_t attempt = 0; attempt < n; ++attempt) {
        const auto& c = ordered[(startIdx + attempt) % n];
        if (try_duplicate_from_donor(c.pid, c.handleValue)) {
            g_cachedDonor.donorPid = m_donorPid;
            g_cachedDonor.donorHandleValue = m_donorHandleValue;
            g_cachedDonor.valid = true;
            set_error("ok");
            return true;
        }
    }

    g_cachedDonor = {};
    set_error("all candidates failed OpenProcess(DUP_HANDLE)/Duplicate/pid-check");
    return false;
#else
    set_error("hijack requires Windows");
    return false;
#endif
}

// ═══════════════════════════════════════════════════════════════════════
// ensure_handle: re-duplicate if our CS2 handle was closed.
// ═══════════════════════════════════════════════════════════════════════

} // namespace real::cs2::hijack

