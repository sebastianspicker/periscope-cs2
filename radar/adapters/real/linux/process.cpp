// process.cpp — Linux process enumeration and deep /proc inspection.

#include "real/linux/process.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <dirent.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace real::linux::proc {
namespace {

bool is_all_digits(const std::string& s) {
    return !s.empty() &&
           std::all_of(s.begin(), s.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

std::string read_file_raw(const std::string& path, size_t max_bytes = 1 << 20) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string data(max_bytes, '\0');
    f.read(data.data(), static_cast<std::streamsize>(max_bytes));
    data.resize(static_cast<size_t>(f.gcount()));
    return data;
}

std::string read_file_line(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    std::getline(f, line);
    return line;
}

} // namespace

// ── Pure parsers (always available) ────────────────────────────────

bool parse_stat_line(const std::string& stat_line, ProcessInfo& out) noexcept {
    if (stat_line.empty()) return false;

    // Format: pid (comm) state ppid ... starttime ...
    // comm may contain spaces/parens — use first '(' and last ')'.
    auto open_paren = stat_line.find('(');
    auto close_paren = stat_line.rfind(')');
    if (open_paren == std::string::npos || close_paren == std::string::npos ||
        close_paren <= open_paren) {
        return false;
    }

    // pid before '('
    try {
        out.pid = static_cast<uint32_t>(
            std::stoul(stat_line.substr(0, open_paren)));
    } catch (...) {
        return false;
    }
    out.name = stat_line.substr(open_paren + 1, close_paren - open_paren - 1);

    std::istringstream rest(stat_line.substr(close_paren + 2));
    // fields after comm: state ppid pgrp session tty_nr tpgid flags
    // minflt cminflt majflt cmajflt utime stime cutime cstime priority
    // nice num_threads itrealvalue starttime ...
    rest >> out.state >> out.ppid;
    long pgrp = 0, session = 0, tty = 0, tpgid = 0;
    unsigned long flags = 0, minflt = 0, cminflt = 0, majflt = 0, cmajflt = 0;
    unsigned long utime = 0, stime = 0;
    long cutime = 0, cstime = 0, priority = 0, nice = 0;
    long num_threads = 0, itreal = 0;
    unsigned long long starttime = 0;
    rest >> pgrp >> session >> tty >> tpgid >> flags >> minflt >> cminflt >>
        majflt >> cmajflt >> utime >> stime >> cutime >> cstime >> priority >>
        nice >> num_threads >> itreal >> starttime;
    out.threads = num_threads > 0 ? static_cast<uint32_t>(num_threads) : 0;
    out.start_time = static_cast<uint64_t>(starttime);
    out.tgid = out.pid; // refined from status if available
    (void)pgrp;
    (void)session;
    (void)tty;
    (void)tpgid;
    (void)flags;
    (void)minflt;
    (void)cminflt;
    (void)majflt;
    (void)cmajflt;
    (void)utime;
    (void)stime;
    (void)cutime;
    (void)cstime;
    (void)priority;
    (void)nice;
    (void)itreal;
    return true;
}

bool parse_status_uid_line(const std::string& line, uint32_t& uid,
                           uint32_t& euid) noexcept {
    // "Uid:\t1\t2\t3\t4"
    if (line.rfind("Uid:", 0) != 0) return false;
    unsigned r = 0, e = 0, s = 0, f = 0;
    if (std::sscanf(line.c_str(), "Uid:\t%u\t%u\t%u\t%u", &r, &e, &s, &f) < 1 &&
        std::sscanf(line.c_str(), "Uid: %u %u %u %u", &r, &e, &s, &f) < 1) {
        return false;
    }
    uid = r;
    euid = e;
    (void)s;
    (void)f;
    return true;
}

// ── Enumeration ────────────────────────────────────────────────────

Result<std::vector<ProcessInfo>> enumerate_all() noexcept {
#if LR_PLATFORM_LINUX
    std::vector<ProcessInfo> processes;
    DIR* proc = ::opendir("/proc");
    if (!proc) return Result<std::vector<ProcessInfo>>({}, "Cannot open /proc");

    while (dirent* entry = ::readdir(proc)) {
        if (!is_all_digits(entry->d_name)) continue;
        uint32_t pid = static_cast<uint32_t>(std::stoul(entry->d_name));
        auto info = get_info(pid);
        if (info) processes.push_back(*info);
    }
    ::closedir(proc);
    return processes;
#else
    return Result<std::vector<ProcessInfo>>({}, "enumerate_all requires Linux");
#endif
}

Result<uint32_t> find_by_name(const std::string& name) noexcept {
    auto all = find_all_by_name(name);
    if (!all) return Result<uint32_t>(0, all.error_msg);
    if (all->empty()) return Result<uint32_t>(0, "Process not found: " + name);
    return (*all)[0];
}

Result<std::vector<uint32_t>> find_all_by_name(const std::string& name) noexcept {
    auto all = enumerate_all();
    if (!all) return Result<std::vector<uint32_t>>({}, all.error_msg);

    std::string needle = name;
    std::transform(needle.begin(), needle.end(), needle.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::vector<uint32_t> hits;
    for (const auto& p : *all) {
        std::string n = p.name;
        std::transform(n.begin(), n.end(), n.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (n.find(needle) != std::string::npos) hits.push_back(p.pid);
    }
    return hits;
}

Result<ProcessInfo> get_info(uint32_t pid) noexcept {
    ProcessInfo info;
    info.pid = pid;

#if LR_PLATFORM_LINUX
    char stat_path[64];
    std::snprintf(stat_path, sizeof(stat_path), "/proc/%u/stat", pid);
    std::string stat_line = read_file_line(stat_path);
    if (stat_line.empty()) {
        return Result<ProcessInfo>({}, "Process not found: " + std::to_string(pid));
    }
    if (!parse_stat_line(stat_line, info)) {
        return Result<ProcessInfo>({}, "Cannot parse stat");
    }
    info.pid = pid; // trust directory pid

    char exe_link[64];
    std::snprintf(exe_link, sizeof(exe_link), "/proc/%u/exe", pid);
    char exe_path[4096] = {};
    ssize_t len = ::readlink(exe_link, exe_path, sizeof(exe_path) - 1);
    if (len > 0) info.exe_path.assign(exe_path, static_cast<size_t>(len));

    auto cmd = get_cmdline(pid);
    if (cmd) info.cmdline = *cmd;

    char status_path[64];
    std::snprintf(status_path, sizeof(status_path), "/proc/%u/status", pid);
    std::ifstream status(status_path);
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind("Uid:", 0) == 0) {
            parse_status_uid_line(line, info.uid, info.euid);
        } else if (line.rfind("Gid:", 0) == 0) {
            unsigned g = 0;
            std::sscanf(line.c_str(), "Gid:\t%u", &g);
            info.gid = g;
        } else if (line.rfind("Tgid:", 0) == 0) {
            unsigned t = 0;
            std::sscanf(line.c_str(), "Tgid:\t%u", &t);
            info.tgid = t;
        } else if (line.rfind("Threads:", 0) == 0) {
            unsigned t = 0;
            std::sscanf(line.c_str(), "Threads:\t%u", &t);
            info.threads = t;
        } else if (line.rfind("VmSize:", 0) == 0) {
            unsigned long v = 0;
            std::sscanf(line.c_str(), "VmSize:\t%lu", &v);
            info.vsize = static_cast<uint64_t>(v) * 1024ULL; // kB → bytes
        } else if (line.rfind("VmRSS:", 0) == 0) {
            unsigned long v = 0;
            std::sscanf(line.c_str(), "VmRSS:\t%lu", &v);
            // store as pages approx (4k)
            info.rss_pages = (static_cast<uint64_t>(v) * 1024ULL) / 4096ULL;
        }
    }
    return info;
#else
    // Host path: still exercise parse_stat_line via synthetic empty failure.
    (void)info;
    return Result<ProcessInfo>({}, "get_info requires Linux");
#endif
}

Result<bool> is_alive(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%u", pid);
    struct stat st {};
    return ::stat(path, &st) == 0;
#else
    (void)pid;
    return Result<bool>(false, "is_alive requires Linux");
#endif
}

Result<std::string> get_cmdline(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%u/cmdline", pid);
    std::string raw = read_file_raw(path, 8192);
    if (raw.empty() && !is_alive(pid)) {
        return Result<std::string>({}, "cmdline unavailable");
    }
    for (size_t i = 0; i + 1 < raw.size(); ++i) {
        if (raw[i] == '\0') raw[i] = ' ';
    }
    while (!raw.empty() && (raw.back() == '\0' || raw.back() == ' ')) raw.pop_back();
    return raw;
#else
    (void)pid;
    return Result<std::string>({}, "get_cmdline requires Linux");
#endif
}

Result<std::string> get_exe_path(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char link[64];
    std::snprintf(link, sizeof(link), "/proc/%u/exe", pid);
    char buf[4096] = {};
    ssize_t n = ::readlink(link, buf, sizeof(buf) - 1);
    if (n < 0) return Result<std::string>({}, "readlink exe failed");
    return std::string(buf, static_cast<size_t>(n));
#else
    (void)pid;
    return Result<std::string>({}, "get_exe_path requires Linux");
#endif
}

Result<std::string> get_cwd(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char link[64];
    std::snprintf(link, sizeof(link), "/proc/%u/cwd", pid);
    char buf[4096] = {};
    ssize_t n = ::readlink(link, buf, sizeof(buf) - 1);
    if (n < 0) return Result<std::string>({}, "readlink cwd failed");
    return std::string(buf, static_cast<size_t>(n));
#else
    (void)pid;
    return Result<std::string>({}, "get_cwd requires Linux");
#endif
}

Result<std::string> get_environ_raw(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%u/environ", pid);
    return read_file_raw(path, 1 << 20);
#else
    (void)pid;
    return Result<std::string>({}, "get_environ_raw requires Linux");
#endif
}

Result<std::vector<MemoryMapping>> get_mappings(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char maps_path[64];
    std::snprintf(maps_path, sizeof(maps_path), "/proc/%u/maps", pid);
    std::ifstream maps(maps_path);
    if (!maps) {
        return Result<std::vector<MemoryMapping>>(
            {}, std::string("Cannot open ") + maps_path);
    }
    std::vector<MemoryMapping> out;
    std::string line;
    while (std::getline(maps, line)) {
        MemoryMapping mm;
        if (maps::parse_line(line, mm)) out.push_back(std::move(mm));
    }
    return out;
#else
    (void)pid;
    return Result<std::vector<MemoryMapping>>({}, "get_mappings requires Linux");
#endif
}

Result<uint64_t> find_module_base(uint32_t pid,
                                  const std::string& module_name) noexcept {
    auto mappings = get_mappings(pid);
    if (!mappings) return Result<uint64_t>(0, mappings.error_msg);
    const auto* m = maps::find_by_name(*mappings, module_name);
    if (!m) return Result<uint64_t>(0, "Module not found: " + module_name);
    return m->start;
}

Result<uint64_t> find_module_base_exec(uint32_t pid,
                                       const std::string& module_name) noexcept {
    auto mappings = get_mappings(pid);
    if (!mappings) return Result<uint64_t>(0, mappings.error_msg);
    const auto* m = maps::find_exec_by_name(*mappings, module_name);
    if (!m) return Result<uint64_t>(0, "Exec module not found: " + module_name);
    return m->start;
}

Result<std::vector<ThreadInfo>> get_threads(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%u/task", pid);
    DIR* d = ::opendir(path);
    if (!d) return Result<std::vector<ThreadInfo>>({}, "Cannot open task/");

    std::vector<ThreadInfo> threads;
    while (dirent* e = ::readdir(d)) {
        if (!is_all_digits(e->d_name)) continue;
        ThreadInfo t;
        t.tid = static_cast<uint32_t>(std::stoul(e->d_name));
        char statp[96];
        std::snprintf(statp, sizeof(statp), "/proc/%u/task/%u/stat", pid, t.tid);
        ProcessInfo tmp;
        if (parse_stat_line(read_file_line(statp), tmp)) {
            t.name = tmp.name;
            t.state = tmp.state;
        }
        char statusp[96];
        std::snprintf(statusp, sizeof(statusp), "/proc/%u/task/%u/status", pid,
                      t.tid);
        std::ifstream st(statusp);
        std::string line;
        while (std::getline(st, line)) {
            if (line.rfind("Name:", 0) == 0) {
                auto pos = line.find('\t');
                if (pos != std::string::npos) t.name = line.substr(pos + 1);
            }
        }
        threads.push_back(std::move(t));
    }
    ::closedir(d);
    return threads;
#else
    (void)pid;
    return Result<std::vector<ThreadInfo>>({}, "get_threads requires Linux");
#endif
}

Result<std::vector<FdInfo>> get_fds(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%u/fd", pid);
    DIR* d = ::opendir(path);
    if (!d) return Result<std::vector<FdInfo>>({}, "Cannot open fd/");

    std::vector<FdInfo> fds;
    while (dirent* e = ::readdir(d)) {
        if (!is_all_digits(e->d_name)) continue;
        FdInfo info;
        info.fd = std::atoi(e->d_name);
        char link[96];
        std::snprintf(link, sizeof(link), "/proc/%u/fd/%d", pid, info.fd);
        char buf[512] = {};
        ssize_t n = ::readlink(link, buf, sizeof(buf) - 1);
        if (n > 0) info.target.assign(buf, static_cast<size_t>(n));
        fds.push_back(std::move(info));
    }
    ::closedir(d);
    return fds;
#else
    (void)pid;
    return Result<std::vector<FdInfo>>({}, "get_fds requires Linux");
#endif
}

Result<NamespaceInfo> get_namespaces(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    NamespaceInfo ns;
    auto rd = [&](const char* which) -> std::string {
        char link[96];
        std::snprintf(link, sizeof(link), "/proc/%u/ns/%s", pid, which);
        char buf[128] = {};
        ssize_t n = ::readlink(link, buf, sizeof(buf) - 1);
        if (n > 0) return std::string(buf, static_cast<size_t>(n));
        return {};
    };
    ns.mnt = rd("mnt");
    ns.pid_ns = rd("pid");
    ns.net = rd("net");
    ns.user = rd("user");
    ns.uts = rd("uts");
    ns.ipc = rd("ipc");
    ns.cgroup = rd("cgroup");
    return ns;
#else
    (void)pid;
    return Result<NamespaceInfo>({}, "get_namespaces requires Linux");
#endif
}

Result<std::string> get_cgroup(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%u/cgroup", pid);
    return read_file_raw(path, 65536);
#else
    (void)pid;
    return Result<std::string>({}, "get_cgroup requires Linux");
#endif
}

Result<bool> is_root_process(uint32_t pid) noexcept {
    auto uid = get_uid(pid);
    if (!uid) return Result<bool>(false, uid.error_msg);
    return *uid == 0;
}

Result<uint32_t> get_uid(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char status_path[64];
    std::snprintf(status_path, sizeof(status_path), "/proc/%u/status", pid);
    std::ifstream status(status_path);
    if (!status) return Result<uint32_t>(0, "Cannot open status");
    std::string line;
    while (std::getline(status, line)) {
        uint32_t u = 0, e = 0;
        if (parse_status_uid_line(line, u, e)) return u;
    }
    return Result<uint32_t>(0, "UID not found in status");
#else
    (void)pid;
    return Result<uint32_t>(0, "get_uid requires Linux");
#endif
}

Result<uint32_t> get_euid(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char status_path[64];
    std::snprintf(status_path, sizeof(status_path), "/proc/%u/status", pid);
    std::ifstream status(status_path);
    if (!status) return Result<uint32_t>(0, "Cannot open status");
    std::string line;
    while (std::getline(status, line)) {
        uint32_t u = 0, e = 0;
        if (parse_status_uid_line(line, u, e)) return e;
    }
    return Result<uint32_t>(0, "EUID not found in status");
#else
    (void)pid;
    return Result<uint32_t>(0, "get_euid requires Linux");
#endif
}

} // namespace real::linux::proc
