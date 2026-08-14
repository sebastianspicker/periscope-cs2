// pagemap.hpp — Pure pagemap entry decode + VA→PA helpers.
//
// Portable: no OS syscalls. Unit-tested on every host. Used by memory.cpp
// and page_walk.cpp on Linux for /proc/[pid]/pagemap interpretation.
//
// Reference: Documentation/admin-guide/mm/pagemap.rst

#pragma once

#include <cstddef>
#include <cstdint>

namespace real::linux::pagemap {

// Linux pagemap entry bit layout (64-bit, since kernel 3.0+):
//   bits 0-54  : PFN if present, or swap type/offset if swapped
//   bit  55    : soft-dirty
//   bit  56    : exclusively mapped
//   bit  57-60 : reserved
//   bit  61    : file-page or shared-anon
//   bit  62    : page swapped
//   bit  63    : page present
constexpr uint64_t kPresentBit     = 1ULL << 63;
constexpr uint64_t kSwappedBit     = 1ULL << 62;
constexpr uint64_t kFilePageBit    = 1ULL << 61;
constexpr uint64_t kExclusiveBit   = 1ULL << 56;
constexpr uint64_t kSoftDirtyBit   = 1ULL << 55;
constexpr uint64_t kPfnMask        = (1ULL << 55) - 1ULL; // bits 0-54

// x86-64 4-level page-table constants (also used by page_walk).
constexpr uint64_t kPagePresent    = 0x001;
constexpr uint64_t kPageRw         = 0x002;
constexpr uint64_t kPageUser       = 0x004;
constexpr uint64_t kPagePwt        = 0x008;
constexpr uint64_t kPagePcd        = 0x010;
constexpr uint64_t kPageAccessed   = 0x020;
constexpr uint64_t kPageDirty      = 0x040;
constexpr uint64_t kPageLarge      = 0x080; // PS bit: 2MiB/1GiB
constexpr uint64_t kPageGlobal     = 0x100;
constexpr uint64_t kPageNx         = 1ULL << 63;
constexpr uint64_t kPhysAddrMask   = 0x000FFFFFFFFFF000ULL;
constexpr uint64_t kDefaultPageSz  = 4096ULL;
constexpr uint64_t kLargePageSz    = 2ULL * 1024 * 1024;
constexpr uint64_t kHugePageSz     = 1ULL * 1024 * 1024 * 1024;

struct Entry {
    uint64_t raw = 0;
    uint64_t pfn = 0;
    bool present = false;
    bool swapped = false;
    bool file_page = false;
    bool exclusively_mapped = false;
    bool soft_dirty = false;
    uint8_t swap_type = 0;
    uint64_t swap_offset = 0;
};

/// Decode a single 64-bit /proc/pagemap entry. Pure function.
inline Entry decode(uint64_t raw) noexcept {
    Entry e;
    e.raw = raw;
    e.present = (raw & kPresentBit) != 0;
    e.swapped = (raw & kSwappedBit) != 0;
    e.file_page = (raw & kFilePageBit) != 0;
    e.exclusively_mapped = (raw & kExclusiveBit) != 0;
    e.soft_dirty = (raw & kSoftDirtyBit) != 0;
    if (e.present) {
        e.pfn = raw & kPfnMask;
    } else if (e.swapped) {
        e.swap_type = static_cast<uint8_t>(raw & 0x1FULL);
        e.swap_offset = (raw >> 5) & ((1ULL << 50) - 1ULL);
    }
    return e;
}

/// File offset of the pagemap entry for a virtual address.
inline uint64_t entry_file_offset(uint64_t virt_addr,
                                  uint64_t page_size = kDefaultPageSz) noexcept {
    if (page_size == 0) page_size = kDefaultPageSz;
    return (virt_addr / page_size) * sizeof(uint64_t);
}

/// Physical address from a present pagemap entry + VA page offset.
/// Returns 0 if the page is not present.
inline uint64_t to_physical(const Entry& e, uint64_t virt_addr,
                            uint64_t page_size = kDefaultPageSz) noexcept {
    if (!e.present || page_size == 0) return 0;
    return (e.pfn * page_size) + (virt_addr & (page_size - 1));
}

/// Convenience: decode raw entry and translate VA → PA in one step.
/// Returns 0 if not present.
inline uint64_t raw_to_physical(uint64_t raw, uint64_t virt_addr,
                                uint64_t page_size = kDefaultPageSz) noexcept {
    return to_physical(decode(raw), virt_addr, page_size);
}

// ── x86-64 page-table index helpers ────────────────────────────────

inline unsigned pml4_index(uint64_t va) noexcept {
    return static_cast<unsigned>((va >> 39) & 0x1FF);
}
inline unsigned pdpt_index(uint64_t va) noexcept {
    return static_cast<unsigned>((va >> 30) & 0x1FF);
}
inline unsigned pd_index(uint64_t va) noexcept {
    return static_cast<unsigned>((va >> 21) & 0x1FF);
}
inline unsigned pt_index(uint64_t va) noexcept {
    return static_cast<unsigned>((va >> 12) & 0x1FF);
}
inline uint64_t page_offset_4k(uint64_t va) noexcept {
    return va & 0xFFFULL;
}
inline uint64_t page_offset_2m(uint64_t va) noexcept {
    return va & (kLargePageSz - 1);
}
inline uint64_t page_offset_1g(uint64_t va) noexcept {
    return va & (kHugePageSz - 1);
}

/// Extract physical frame base from a PTE/PDE/PDPTE (clears flags).
inline uint64_t pte_phys_base(uint64_t entry) noexcept {
    return entry & kPhysAddrMask;
}

/// True if a page-table entry is present.
inline bool pte_present(uint64_t entry) noexcept {
    return (entry & kPagePresent) != 0;
}

/// True if a PDE/PDPTE maps a large page (PS bit).
inline bool pte_large(uint64_t entry) noexcept {
    return (entry & kPageLarge) != 0;
}

/// Walk result for a single VA using pre-fetched page-table entries.
struct WalkResult {
    bool ok = false;
    uint64_t phys = 0;
    uint64_t page_size = 0; // 4K / 2M / 1G
    uint64_t pml4e = 0;
    uint64_t pdpte = 0;
    uint64_t pde = 0;
    uint64_t pte = 0;
    const char* fail_stage = nullptr;
};

/// Pure software page-table walk given the four levels already read.
/// Caller supplies the raw entries at the correct indices for `va`.
/// pdpte/pde/pte may be 0 if a higher level was large or not present.
inline WalkResult walk_from_entries(uint64_t va, uint64_t pml4e, uint64_t pdpte,
                                    uint64_t pde, uint64_t pte) noexcept {
    WalkResult r;
    r.pml4e = pml4e;
    r.pdpte = pdpte;
    r.pde = pde;
    r.pte = pte;

    if (!pte_present(pml4e)) {
        r.fail_stage = "pml4e";
        return r;
    }
    if (!pte_present(pdpte)) {
        r.fail_stage = "pdpte";
        return r;
    }
    // 1 GiB page at PDPT level
    if (pte_large(pdpte)) {
        r.ok = true;
        r.page_size = kHugePageSz;
        r.phys = pte_phys_base(pdpte) + page_offset_1g(va);
        return r;
    }
    if (!pte_present(pde)) {
        r.fail_stage = "pde";
        return r;
    }
    // 2 MiB page at PD level
    if (pte_large(pde)) {
        r.ok = true;
        r.page_size = kLargePageSz;
        r.phys = pte_phys_base(pde) + page_offset_2m(va);
        return r;
    }
    if (!pte_present(pte)) {
        r.fail_stage = "pte";
        return r;
    }
    r.ok = true;
    r.page_size = kDefaultPageSz;
    r.phys = pte_phys_base(pte) + page_offset_4k(va);
    return r;
}

} // namespace real::linux::pagemap
