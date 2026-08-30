// process.hpp — Linux process enumeration and deep inspection via /proc.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"
#include "real/linux/maps_parse.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::linux::proc {

struct ProcessInfo {
    uint32_t pid = 0;
    uint32_t ppid = 0;
    uint32_t tgid = 0;
    std::string name;
    std::string exe_path;
    std::string cmdline;
    uint64_t start_time = 0; // clock ticks from boot (stat field 22)
    char state = '?';       // R S D Z T t W X x K W
    uint32_t uid = 0;
    uint32_t euid = 0;
    uint32_t gid = 0;
    uint32_t threads = 0;
    uint64_t vsize = 0;
    uint64_t rss_pages = 0;
};

struct ThreadInfo {
    uint32_t tid = 0;
    std::string name;
    char state = '?';
    uint64_t start_stack = 0;
};

struct FdInfo {
    int fd = -1;
    std::string target; // readlink of /proc/pid/fd/N
};

struct NamespaceInfo {
    std::string mnt;
    std::string pid_ns;
    std::string net;
    std::string user;
    std::string uts;
    std::string ipc;
    std::string cgroup;
};

using MemoryMapping = maps::Mapping;

// ── Enumeration ────────────────────────────────────────────────────

Result<std::vector<ProcessInfo>> enumerate_all() noexcept;
Result<uint32_t> find_by_name(const std::string& name) noexcept;
Result<std::vector<uint32_t>> find_all_by_name(const std::string& name) noexcept;

// ── Single process ─────────────────────────────────────────────────

Result<ProcessInfo> get_info(uint32_t pid) noexcept;
Result<bool> is_alive(uint32_t pid) noexcept;
Result<std::string> get_cmdline(uint32_t pid) noexcept;
Result<std::string> get_exe_path(uint32_t pid) noexcept;
Result<std::string> get_cwd(uint32_t pid) noexcept;
Result<std::string> get_environ_raw(uint32_t pid) noexcept;

// ── Memory maps ────────────────────────────────────────────────────

Result<std::vector<MemoryMapping>> get_mappings(uint32_t pid) noexcept;
Result<uint64_t> find_module_base(uint32_t pid,
                                  const std::string& module_name) noexcept;
Result<uint64_t> find_module_base_exec(uint32_t pid,
                                       const std::string& module_name) noexcept;

// ── Threads / FDs / namespaces ─────────────────────────────────────

Result<std::vector<ThreadInfo>> get_threads(uint32_t pid) noexcept;
Result<std::vector<FdInfo>> get_fds(uint32_t pid) noexcept;
Result<NamespaceInfo> get_namespaces(uint32_t pid) noexcept;
Result<std::string> get_cgroup(uint32_t pid) noexcept;

// ── Credentials ────────────────────────────────────────────────────

Result<bool> is_root_process(uint32_t pid) noexcept;
Result<uint32_t> get_uid(uint32_t pid) noexcept;
Result<uint32_t> get_euid(uint32_t pid) noexcept;

// ── Pure helpers (unit-tested on every host) ───────────────────────

/// Parse /proc/[pid]/stat line into ProcessInfo fields (name/state/ppid/start).
bool parse_stat_line(const std::string& stat_line, ProcessInfo& out) noexcept;

/// Parse Uid: line from /proc/[pid]/status ("Uid:\treal euid suid fsuid").
bool parse_status_uid_line(const std::string& line, uint32_t& uid,
                           uint32_t& euid) noexcept;

} // namespace real::linux::proc
