// hijack_reader_internal.hpp — shared state/helpers for hijack_reader*.cpp TUs.
#pragma once

#include "real/cs2/hijack_reader.hpp"
#include "real/win/api_table.hpp"
#include "real/win/syscall_helper.hpp"

#include <cstdint>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

namespace real::cs2::hijack {
namespace detail {

inline uint64_t xor_shift(uint64_t& state) noexcept {
    state ^= state << 13;
    state ^= state << 17;
    state ^= state << 5;
    return state;
}

struct CachedDonorInfo {
    uint32_t donorPid = 0;
    uint64_t donorHandleValue = 0;
    bool valid = false;
};

// Defined in hijack_reader_discover.cpp
extern CachedDonorInfo g_cachedDonor;
extern int g_ntqsi40_call_count;

#if LR_PLATFORM_WINDOWS
bool is_preferred_donor_basename(const char* base) noexcept;
std::vector<uint32_t> collect_preferred_donor_pids(uint32_t cs2Pid) noexcept;
bool pid_in_set(const std::vector<uint32_t>& pids, uint32_t pid) noexcept;
HANDLE open_donor_for_dup(uint32_t donorPid) noexcept;
bool handle_has_vm_read(HANDLE h) noexcept;
NTSTATUS duplicate_cs2_handle(HANDLE donorProc, HANDLE sourceHandle, HANDLE* outDupe) noexcept;
#endif

}  // namespace detail

using detail::xor_shift;
using detail::g_cachedDonor;
using detail::g_ntqsi40_call_count;

}  // namespace real::cs2::hijack