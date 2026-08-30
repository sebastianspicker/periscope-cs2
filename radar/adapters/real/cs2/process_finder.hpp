// process_finder.hpp — Enhanced CS2 process & module discovery.
//
// Uses API table (PEB+EAT resolved) for all operations. Opens CS2
// with PROCESS_QUERY_LIMITED_INFORMATION only — NOT VM_READ.
// VM_READ comes from the hijack reader (T0.4), not from here.
//
// Always prefer EnumProcessModulesEx with LIST_MODULES_ALL,
// fallback to CreateToolhelp32Snapshot with TH32CS_SNAPMODULE.
//
// Reference: Periscope prototype/src/product_bootstrap.cpp,
//            prototype/src/memory/reader_hijack.cpp

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace real::cs2 {

/// Minimal process info for CS2.
/// No VM_READ handle — only QUERY_LIMITED_INFORMATION.
struct Cs2ProcessInfo {
    uint32_t pid{};
    uint64_t hProc{};     // PROCESS_QUERY_LIMITED_INFORMATION only
};

/// A loaded module in the CS2 process.
struct Cs2ModuleInfo {
    std::string name;     // "client.dll", "engine2.dll", etc.
    uint64_t base{};      // Base address in CS2's address space
    size_t size{};        // SizeOfImage (from PE header, corrected)
};

/// Find CS2 by name. Returns nullopt if not running.
std::optional<Cs2ProcessInfo> find_cs2() noexcept;

/// Open CS2 with PROCESS_QUERY_LIMITED_INFORMATION only.
std::optional<Cs2ProcessInfo> open_cs2_limited(uint32_t pid) noexcept;

/// Enumerate all modules in CS2.
/// Path A: EnumProcessModulesEx + GetModuleInformation
/// Path B: CreateToolhelp32Snapshot with TH32CS_SNAPMODULE
std::vector<Cs2ModuleInfo> enumerate_modules(
    uint32_t pid, uint64_t hProc) noexcept;

/// Fix SizeOfImage by reading PE headers remotely.
size_t correct_image_size(uint32_t pid, uint64_t hProc,
                          uint64_t baseAddr) noexcept;

/// Check if CS2 is still alive.
bool is_cs2_alive(uint32_t pid) noexcept;

} // namespace real::cs2
