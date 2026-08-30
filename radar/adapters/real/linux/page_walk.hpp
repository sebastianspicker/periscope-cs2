// page_walk.hpp — Software x86-64 4-level page-table walk via physical reads.
//
// Requires a physical-memory backend ( /dev/mem or /dev/aclab ). Used when
// pagemap is unavailable (foreign process without CAP_SYS_ADMIN, kernel VA).

#pragma once

#include "real/error.hpp"
#include "real/linux/pagemap.hpp"

#include <cstdint>
#include <functional>
#include <vector>

namespace real::linux::page_walk {

using PhysReadFn = std::function<Result<std::vector<uint8_t>>(uint64_t phys,
                                                              size_t size)>;

/// Walk `va` under directory table base `cr3` using `phys_read` for each level.
Result<pagemap::WalkResult> walk(uint64_t cr3, uint64_t va,
                                 const PhysReadFn& phys_read) noexcept;

/// Read a single 8-byte page-table entry at physical address.
Result<uint64_t> read_pte(uint64_t phys_entry_addr,
                          const PhysReadFn& phys_read) noexcept;

/// Pure: compute physical address of PTE slot for index within a table page.
inline uint64_t table_slot_phys(uint64_t table_phys, unsigned index) noexcept {
    return (table_phys & pagemap::kPhysAddrMask) +
           static_cast<uint64_t>(index) * 8ULL;
}

} // namespace real::linux::page_walk
