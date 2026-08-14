// maps_parse.hpp — Pure /proc/[pid]/maps line parser.
// Portable: no OS syscalls. Unit-tested on every host.

#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace real::linux::maps {

struct Mapping {
    uint64_t start = 0;
    uint64_t end = 0;
    bool readable = false;
    bool writable = false;
    bool executable = false;
    bool shared = false; // 's' vs 'p'
    uint64_t offset = 0;
    uint32_t dev_major = 0;
    uint32_t dev_minor = 0;
    uint64_t inode = 0;
    std::string pathname;
};

/// Parse one /proc/[pid]/maps line. Returns false on malformed input.
inline bool parse_line(const char* line, Mapping& out) noexcept {
    if (!line || !*line) return false;

    unsigned long long start = 0, end = 0, off = 0, inode = 0;
    unsigned dev_maj = 0, dev_min = 0;
    char perms[8] = {};
    // pathname may be empty (anonymous) or contain spaces (rare) — capture rest.
    int n = std::sscanf(line, "%llx-%llx %7s %llx %x:%x %llu",
                        &start, &end, perms, &off, &dev_maj, &dev_min, &inode);
    if (n < 7) return false;
    if (end < start) return false;

    out = Mapping{};
    out.start = static_cast<uint64_t>(start);
    out.end = static_cast<uint64_t>(end);
    out.offset = static_cast<uint64_t>(off);
    out.dev_major = dev_maj;
    out.dev_minor = dev_min;
    out.inode = static_cast<uint64_t>(inode);
    out.readable = perms[0] == 'r';
    out.writable = perms[1] == 'w';
    out.executable = perms[2] == 'x';
    out.shared = perms[3] == 's';

    // Pathname starts after the seventh field; skip leading whitespace.
    const char* p = line;
    // Advance past 6 spaces-separated tokens roughly by walking.
    int fields = 0;
    bool in = false;
    for (; *p; ++p) {
        if (*p != ' ' && *p != '\t') {
            if (!in) {
                ++fields;
                in = true;
            }
        } else {
            in = false;
            if (fields >= 6) {
                // at end of inode field area — continue to skip remaining of inode
            }
        }
        if (fields >= 6 && (*p == ' ' || *p == '\t')) {
            // find start of pathname
            while (*p == ' ' || *p == '\t') ++p;
            // skip inode digits if still on them
            // actually after field 6 (inode) next non-space is path
            break;
        }
    }
    // More reliable: find pathname with sscanf trailing %n after 7 fields.
    int path_off = 0;
    std::sscanf(line, "%*llx-%*llx %*s %*llx %*x:%*x %*llu %n", &path_off);
    if (path_off > 0 && line[path_off]) {
        // trim trailing newline/CR
        std::string path(line + path_off);
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r' ||
                                 path.back() == ' ')) {
            path.pop_back();
        }
        out.pathname = std::move(path);
    }
    return true;
}

inline bool parse_line(const std::string& line, Mapping& out) noexcept {
    return parse_line(line.c_str(), out);
}

/// Size of a mapping in bytes.
inline uint64_t size_bytes(const Mapping& m) noexcept {
    return m.end >= m.start ? (m.end - m.start) : 0;
}

/// First mapping whose pathname contains `needle` (case-sensitive).
inline const Mapping* find_by_name(const std::vector<Mapping>& maps,
                                   const std::string& needle) noexcept {
    if (needle.empty()) return nullptr;
    for (const auto& m : maps) {
        if (m.pathname.find(needle) != std::string::npos) return &m;
    }
    return nullptr;
}

/// First executable mapping whose pathname contains `needle`.
inline const Mapping* find_exec_by_name(const std::vector<Mapping>& maps,
                                        const std::string& needle) noexcept {
    if (needle.empty()) return nullptr;
    for (const auto& m : maps) {
        if (m.executable && m.pathname.find(needle) != std::string::npos)
            return &m;
    }
    return nullptr;
}

/// True if address falls inside the mapping.
inline bool contains(const Mapping& m, uint64_t addr) noexcept {
    return addr >= m.start && addr < m.end;
}

} // namespace real::linux::maps
