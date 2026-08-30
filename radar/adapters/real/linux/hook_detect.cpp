// hook_detect.cpp — LD_PRELOAD / maps injection scar detection.

#include "real/linux/hook_detect.hpp"
#include "real/linux/maps_parse.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace real::linux::hook_detect {

std::vector<std::string> split_preload_list(const std::string& value) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : value) {
        if (c == ':' || c == ' ' || c == '\t' || c == '\n') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool pathname_suspicious(const std::string& pathname,
                         const std::vector<std::string>& allow_suffixes) {
    if (pathname.empty() || pathname[0] == '[') return false; // [stack]/vdso]
    // Anonymous executable regions already handled by caller via empty path.
    static const char* kRisk[] = {
        "/tmp/", "/dev/shm/", "/home/", "memfd:", "(deleted)",
        "/var/tmp/", "/run/user/",
    };
    for (const char* r : kRisk) {
        if (pathname.find(r) != std::string::npos) {
            // Allow-list suffix override
            for (const auto& a : allow_suffixes) {
                if (pathname.size() >= a.size() &&
                    pathname.compare(pathname.size() - a.size(), a.size(), a) ==
                        0) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

Result<HookReport> inspect_self() noexcept {
    HookReport r;

    const char* preload = std::getenv("LD_PRELOAD");
    if (preload && *preload) {
        r.ld_preload_set = true;
        r.ld_preload_value = preload;
        r.signals.push_back(std::string("LD_PRELOAD=") + preload);
    }
    const char* lpath = std::getenv("LD_LIBRARY_PATH");
    if (lpath && *lpath) {
        r.ld_library_path_set = true;
        r.signals.push_back(std::string("LD_LIBRARY_PATH=") + lpath);
    }

#if LR_PLATFORM_LINUX
    {
        std::ifstream f("/etc/ld.so.preload");
        if (f) {
            std::string content((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());
            // ignore whitespace-only
            bool nonempty = false;
            for (char c : content) {
                if (c != ' ' && c != '\n' && c != '\t' && c != '\r') {
                    nonempty = true;
                    break;
                }
            }
            if (nonempty) {
                r.ld_so_preload_nonempty = true;
                r.signals.push_back("/etc/ld.so.preload nonempty");
            }
        }
    }

    {
        char path[64];
        std::snprintf(path, sizeof(path), "/proc/self/maps");
        std::ifstream maps_f(path);
        std::string line;
        std::vector<std::string> allow;
        while (std::getline(maps_f, line)) {
            maps::Mapping m;
            if (!maps::parse_line(line, m)) continue;
            if (!m.executable) continue;
            if (pathname_suspicious(m.pathname, allow)) {
                r.suspicious_maps.push_back(m.pathname);
            }
        }
        for (const auto& p : r.suspicious_maps) {
            r.signals.push_back("suspicious map: " + p);
        }
    }
#endif

    r.hooks_suspected = r.ld_preload_set || r.ld_so_preload_nonempty ||
                        !r.suspicious_maps.empty();
    return r;
}

} // namespace real::linux::hook_detect
