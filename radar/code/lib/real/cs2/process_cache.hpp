#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace real::cs2 {

struct ProcessCacheEntry {
    uint32_t pid;
    uint32_t parentPid;
    std::string name;
};

const std::vector<ProcessCacheEntry>& get_cached_process_list();

bool pid_exists(uint32_t pid);

std::optional<uint32_t> get_parent_pid_cached(uint32_t pid);

} // namespace real::cs2
