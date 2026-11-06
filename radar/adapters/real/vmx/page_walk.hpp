// page_walk.hpp — Guest page-table walk and zero-run finder (E67).
//
// The VMX lane can build EPTs but could not walk the guest's own tables.
// This is the pure half: it reads 4 KiB tables through a caller-supplied
// read functor, resolves 4 KiB / 2 MiB / 1 GiB mappings, and reports the
// PTE flags a hook installer needs. `find_zero_run` scans for executable
// all-zero pages suitable for trampoline placement.
//
// Caveats carried from the donor:
//   - zero-run placement is not reserved in the target's VAD manager;
//   - a large-page hit returns the page-table address of the large entry;
//   - physical addresses are bounded by `kMaxPhysical` to reject garbage CR3s.
//
// Pure and host-testable: no platform headers, no I/O.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>

namespace real::vmx::pt {

inline constexpr std::uint64_t kPageSize = 0x1000;
inline constexpr std::uint64_t kLargePageSize = 0x200000;
inline constexpr std::uint64_t kHugePageSize = 0x40000000;
inline constexpr std::uint64_t kMaxPhysical = 0x40'0000'0000ull;

constexpr std::uint64_t kPresent = 1ull << 0;
constexpr std::uint64_t kWrite = 1ull << 1;
constexpr std::uint64_t kLarge = 1ull << 7;
constexpr std::uint64_t kDirty = 1ull << 6;
constexpr std::uint64_t kNoExec = 1ull << 63;

struct PteInfo {
  std::uint64_t va = 0;
  std::uint64_t pte = 0;
  std::uint64_t pte_pa = 0;
  std::uint64_t gpa = 0;
  bool present = false;
  bool large = false;
  bool writable = false;
  bool executable = false;
  bool dirty = false;
};

inline std::uint64_t mask_phys(std::uint64_t pte) {
  return pte & 0x000F'FFFF'FFFF'F000ull;
}

inline bool cr3_ok(std::uint64_t cr3) {
  return cr3 != 0 && (cr3 & 0xFFFull) == 0 && cr3 < kMaxPhysical;
}

template <typename ReadFn>
bool read_page(ReadFn& read, std::uint64_t pa,
               std::array<std::uint64_t, 512>& out) {
  std::array<std::uint8_t, kPageSize> raw{};
  if (pa == 0 || pa >= kMaxPhysical) return false;
  if (!read(pa, raw.data(), raw.size())) return false;
  std::memcpy(out.data(), raw.data(), raw.size());
  return true;
}

/// Resolve `va` in the address space rooted at `cr3`.
template <typename ReadFn>
bool query(ReadFn&& read, std::uint64_t cr3, std::uint64_t va, PteInfo& out) {
  out = PteInfo{};
  out.va = va;
  if (!cr3_ok(cr3)) return false;

  std::array<std::uint64_t, 512> table{};
  if (!read_page(read, cr3 & ~0xFFFull, table)) return false;

  const std::uint64_t pml4e = table[(va >> 39) & 0x1FF];
  if ((pml4e & kPresent) == 0) return false;
  const std::uint64_t pdpt_pa = mask_phys(pml4e);
  if (!read_page(read, pdpt_pa, table)) return false;

  const std::uint64_t pdpte = table[(va >> 30) & 0x1FF];
  if ((pdpte & kPresent) == 0) return false;
  if ((pdpte & kLarge) != 0) {
    out.pte_pa = pdpt_pa + ((va >> 30) & 0x1FF) * 8;
    out.pte = pdpte;
    out.gpa = (pdpte & 0x000F'FFFF'C000'0000ull) | (va & 0x3FFF'FFFFull);
    out.present = true;
    out.large = true;
    out.writable = (pdpte & kWrite) != 0;
    out.executable = (pdpte & kNoExec) == 0;
    out.dirty = (pdpte & kDirty) != 0;
    return true;
  }

  const std::uint64_t pd_pa = mask_phys(pdpte);
  if (!read_page(read, pd_pa, table)) return false;

  const std::uint64_t pde = table[(va >> 21) & 0x1FF];
  if ((pde & kPresent) == 0) return false;
  if ((pde & kLarge) != 0) {
    out.pte_pa = pd_pa + ((va >> 21) & 0x1FF) * 8;
    out.pte = pde;
    out.gpa = (pde & 0x000F'FFFF'FFE0'0000ull) | (va & 0x1F'FFFFull);
    out.present = true;
    out.large = true;
    out.writable = (pde & kWrite) != 0;
    out.executable = (pde & kNoExec) == 0;
    out.dirty = (pde & kDirty) != 0;
    return true;
  }

  const std::uint64_t pt_pa = mask_phys(pde);
  if (!read_page(read, pt_pa, table)) return false;

  const std::uint64_t pte = table[(va >> 12) & 0x1FF];
  out.pte_pa = pt_pa + ((va >> 12) & 0x1FF) * 8;
  out.pte = pte;
  if ((pte & kPresent) == 0) return false;

  out.gpa = mask_phys(pte) | (va & 0xFFFull);
  out.present = true;
  out.writable = (pte & kWrite) != 0;
  out.executable = (pte & kNoExec) == 0;
  out.dirty = (pte & kDirty) != 0;
  return true;
}

/// True when every byte in the page at `pa` is zero.
template <typename ReadFn>
bool page_is_zero(ReadFn& read, std::uint64_t pa) {
  std::array<std::uint8_t, kPageSize> raw{};
  if (!read(pa, raw.data(), raw.size())) return false;
  for (const std::uint8_t byte : raw) {
    if (byte != 0) return false;
  }
  return true;
}

/// Find the first run of `pages` consecutive executable zero pages inside
/// `[lo, hi)`. Returns the virtual address, or 0.
template <typename ReadFn>
std::uint64_t find_zero_run(ReadFn&& read, std::uint64_t cr3, std::uint64_t lo,
                            std::uint64_t hi, std::uint32_t pages,
                            bool executable_only = true) {
  if (!cr3_ok(cr3) || lo >= hi || pages == 0) return 0;
  lo &= ~(kPageSize - 1);
  hi &= ~(kPageSize - 1);

  std::uint32_t run = 0;
  std::uint64_t run_start = 0;
  for (std::uint64_t va = lo; va < hi; va += kPageSize) {
    PteInfo info{};
    const bool usable = query(read, cr3, va, info) && info.present &&
                        info.gpa != 0 && (!executable_only || info.executable);
    if (!usable || !page_is_zero(read, info.gpa & ~(kPageSize - 1))) {
      run = 0;
      continue;
    }
    if (run == 0) run_start = va;
    if (++run >= pages) return run_start;
  }
  return 0;
}

}  // namespace real::vmx::pt
