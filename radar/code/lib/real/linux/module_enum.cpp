// module_enum.cpp — /proc/modules enumeration.

#include "real/linux/module_enum.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace real::linux::modules {

bool parse_modules_line(const std::string& line, KernelModule& out) noexcept {
    // name size refcount deps state [offset]
    // e.g. "ext4 983040 1 - Live 0xffffffffc0a00000"
    if (line.empty()) return false;
    std::istringstream ss(line);
    out = KernelModule{};
    if (!(ss >> out.name >> out.size >> out.refcount)) return false;
    if (!(ss >> out.used_by)) out.used_by = "-";
    if (!(ss >> out.state)) out.state = "Unknown";
    std::string offset_tok;
    if (ss >> offset_tok) {
        unsigned long long off = 0;
        if (offset_tok.rfind("0x", 0) == 0 || offset_tok.rfind("0X", 0) == 0) {
            std::sscanf(offset_tok.c_str(), "%llx", &off);
        } else {
            std::sscanf(offset_tok.c_str(), "%llx", &off);
        }
        out.offset = static_cast<uint64_t>(off);
    }
    return !out.name.empty();
}

Result<std::vector<KernelModule>> enumerate() noexcept {
#if LR_PLATFORM_LINUX
    std::ifstream f("/proc/modules");
    if (!f) {
        return Result<std::vector<KernelModule>>({}, "Cannot open /proc/modules");
    }
    std::vector<KernelModule> out;
    std::string line;
    while (std::getline(f, line)) {
        KernelModule m;
        if (parse_modules_line(line, m)) out.push_back(std::move(m));
    }
    return out;
#else
    return Result<std::vector<KernelModule>>({}, "enumerate requires Linux");
#endif
}

Result<bool> is_loaded(const std::string& name) noexcept {
    auto all = enumerate();
    if (!all) return Result<bool>(false, all.error_msg);
    for (const auto& m : *all) {
        if (m.name == name) return true;
    }
    return false;
}

Result<KernelModule> find(const std::string& name) noexcept {
    auto all = enumerate();
    if (!all) return Result<KernelModule>({}, all.error_msg);
    for (const auto& m : *all) {
        if (m.name == name) return m;
    }
    return Result<KernelModule>({}, "module not found: " + name);
}

} // namespace real::linux::modules
