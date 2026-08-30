#include "real/cs2/process_cache.hpp"
#include "real/win/api_table.hpp"
#include "real/win/xorstr.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

#include <atomic>
#include <vector>

namespace real::cs2 {
namespace {

#if LR_PLATFORM_WINDOWS

struct SystemProcessInfo {
    ULONG NextEntryOffset;
    ULONG NumberOfThreads;
    BYTE Reserved1[48];
    UNICODE_STRING ImageName;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    PVOID InheritedFromUniqueProcessId;
    ULONG HandleCount;
    ULONG SessionId;
    BYTE Reserved3[4];
    SIZE_T PeakVirtualSize;
    SIZE_T VirtualSize;
    ULONG Reserved4;
    SIZE_T PeakWorkingSetSize;
    SIZE_T WorkingSetSize;
    SIZE_T Reserved5;
    SIZE_T QuotaPagedPoolUsage;
    PVOID Reserved6;
    SIZE_T QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage;
    SIZE_T PeakPagefileUsage;
    SIZE_T PrivatePageCount;
    BYTE Reserved7[48];
};

#ifndef PROCESS_QUERY_LIMITED_INFORMATION
#define PROCESS_QUERY_LIMITED_INFORMATION 0x1000
#endif

static std::vector<ProcessCacheEntry> s_cache;
static uint64_t s_lastRefreshMs = 0;
static std::atomic<bool> s_refreshing{false};

struct RefreshGuard {
    std::atomic<bool>& flag;
    RefreshGuard(std::atomic<bool>& f) noexcept : flag(f) {
        flag.store(true, std::memory_order_release);
    }
    ~RefreshGuard() noexcept {
        flag.store(false, std::memory_order_release);
    }
};

uint64_t current_time_ms() {
    auto& api = real::win::g_Api();
    if (api.resolved && api.QueryPerformanceCounter && api.QueryPerformanceFrequency) {
        LARGE_INTEGER count, freq;
        if (api.QueryPerformanceCounter(&count) && api.QueryPerformanceFrequency(&freq)) {
            return static_cast<uint64_t>((count.QuadPart * 1000) / freq.QuadPart);
        }
    }
    return 0;
}

#endif // LR_PLATFORM_WINDOWS

} // anonymous namespace

const std::vector<ProcessCacheEntry>& get_cached_process_list() {
#if LR_PLATFORM_WINDOWS
    auto& api = real::win::g_Api();
    if (!api.resolved || !api.NtQuerySystemInformation) {
        static std::vector<ProcessCacheEntry> empty;
        return empty;
    }

    uint64_t now = current_time_ms();
    uint64_t cacheTTL = 5000 + (now % 4000);

    if (!s_cache.empty() && !s_refreshing.load(std::memory_order_acquire) && (now - s_lastRefreshMs) < cacheTTL) {
        return s_cache;
    }

    if (s_refreshing.load(std::memory_order_acquire)) {
        return s_cache;
    }
    RefreshGuard guard(s_refreshing);

    ULONG bufSize = 0x10000;
    std::vector<uint8_t> buffer(bufSize);
    NTSTATUS status;

    while ((status = api.NtQuerySystemInformation(5, buffer.data(), bufSize, &bufSize)) < 0) {
        buffer.resize(bufSize);
        if (bufSize > 0x100000) {
            return s_cache;
        }
    }

    std::vector<ProcessCacheEntry> new_cache;
    uint8_t* pos = buffer.data();
    while (pos) {
        auto* info = reinterpret_cast<SystemProcessInfo*>(pos);
        ProcessCacheEntry entry;
        entry.pid = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(info->UniqueProcessId));
        entry.parentPid = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(info->InheritedFromUniqueProcessId));

        if (info->ImageName.Buffer && info->ImageName.Length > 0) {
            int wideLen = info->ImageName.Length / static_cast<int>(sizeof(wchar_t));
            if (wideLen > 0) {
                char nameBuf[256]{};
                for (int i = 0; i < wideLen && i < 255; ++i) {
                    nameBuf[i] = static_cast<char>(info->ImageName.Buffer[i]);
                }
                entry.name = nameBuf;
            }
        }

        new_cache.push_back(std::move(entry));

        if (info->NextEntryOffset == 0) break;
        pos += info->NextEntryOffset;
    }

    s_cache = std::move(new_cache);
    s_lastRefreshMs = now;
    return s_cache;
#else
    static std::vector<ProcessCacheEntry> empty;
    return empty;
#endif
}

bool pid_exists(uint32_t pid) {
#if LR_PLATFORM_WINDOWS
    auto& api = real::win::g_Api();
    if (!api.resolved || !api.NtOpenProcess) return false;

    OBJECT_ATTRIBUTES oa = { sizeof(oa) };
    CLIENT_ID cid = { reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid)), nullptr };
    HANDLE h = nullptr;
    NTSTATUS status = api.NtOpenProcess(&h, PROCESS_QUERY_LIMITED_INFORMATION, &oa, &cid);
    if (status >= 0 && h) {
        api.NtClose(h);
        return true;
    }
    return false;
#else
    (void)pid;
    return false;
#endif
}

std::optional<uint32_t> get_parent_pid_cached(uint32_t pid) {
    const auto& list = get_cached_process_list();
    for (const auto& entry : list) {
        if (entry.pid == pid) {
            return entry.parentPid;
        }
    }
    return std::nullopt;
}

} // namespace real::cs2
