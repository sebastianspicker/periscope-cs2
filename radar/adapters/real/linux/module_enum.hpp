// module_enum.hpp — Kernel module enumeration via /proc/modules + /sys/module.
//
// BLUE: detect unexpected modules (aclab, rootkits).
// RED scar: module is always in /proc/modules unless DKOM-hidden (then other scars).

#pragma once

#include "real/error.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::linux::modules {

struct KernelModule {
    std::string name;
    uint64_t size = 0;
    uint32_t refcount = 0;
    std::string used_by; // comma list or "-"
    std::string state;   // Live / Loading / Unloading
    uint64_t offset = 0; // module load address if present
};

Result<std::vector<KernelModule>> enumerate() noexcept;
Result<bool> is_loaded(const std::string& name) noexcept;
Result<KernelModule> find(const std::string& name) noexcept;

/// Pure: parse one /proc/modules line.
bool parse_modules_line(const std::string& line, KernelModule& out) noexcept;

/// Educational denylist of high-risk lab/rootkit names.
inline bool name_is_high_risk(const std::string& name) noexcept {
    return name == "aclab_module" || name == "aclab" || name == "diamorphine" ||
           name == "reptile" || name == "suterusu" || name == "kisskiss";
}

} // namespace real::linux::modules
