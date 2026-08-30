// hook_detect.hpp — Usermode hook / injection scar detection on Linux.
//
// BLUE sensors for:
//   LD_PRELOAD / LD_LIBRARY_PATH environment
//   unexpected shared objects in /proc/self/maps
//   missing execute bit on libc text (crude)
//   /etc/ld.so.preload presence

#pragma once

#include "real/error.hpp"

#include <string>
#include <vector>

namespace real::linux::hook_detect {

struct HookReport {
    bool ld_preload_set = false;
    std::string ld_preload_value;
    bool ld_library_path_set = false;
    bool ld_so_preload_nonempty = false;
    std::vector<std::string> suspicious_maps; // pathnames
    std::vector<std::string> signals;
    bool hooks_suspected = false;
};

Result<HookReport> inspect_self() noexcept;

/// Pure: split LD_PRELOAD-style colon/space list.
std::vector<std::string> split_preload_list(const std::string& value);

/// Pure: return true if pathname looks like an injected .so (heuristic).
bool pathname_suspicious(const std::string& pathname,
                         const std::vector<std::string>& allow_suffixes);

} // namespace real::linux::hook_detect
