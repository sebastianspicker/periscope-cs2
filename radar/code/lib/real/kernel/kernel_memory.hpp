// kernel_memory.hpp — Physical / CR3 / page-walk / DKOM kernel-memory ops.
//
// LESSON: Physical or kernel-memory reads and DKOM leave driver, IOCTL, and
// integrity scars. HVCI, vulnerable-driver blocklists, callbacks, and pool
// scanning are the corresponding anti-cheat mitigations.
//
// All APIs are operational logic paths. They may fail with explicit OS /
// privilege errors when hardware or driver rights are missing — never as
// empty deferred placeholders that pretend the operation ran.

#pragma once

#include "real/error.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace real::kernel::mem {

// ── Operational APIs ───────────────────────────────────────────────

Result<std::vector<std::uint8_t>> read_physical(std::uint64_t phys_addr, std::size_t size);
Result<void> write_physical(std::uint64_t phys_addr, const std::vector<std::uint8_t>& data);
Result<std::vector<std::uint8_t>> read_system_memory(std::uint64_t addr, std::size_t size);
Result<std::uint64_t> virtual_to_physical_kernel(std::uint64_t virt_addr);
Result<std::uint64_t> get_process_cr3(std::uint32_t pid);
Result<std::vector<std::uint8_t>> read_process_memory_by_cr3(std::uint64_t cr3,
                                                              std::uint64_t address,
                                                              std::size_t size);
Result<void> hide_process_eprocess(std::uint32_t pid);
Result<void> steal_token(std::uint32_t target_pid, std::uint32_t source_pid);

// ── Pure page-walk helpers (no I/O; unit-testable offline) ──────────

/// x64 4-level paging constants.
constexpr std::uint64_t kPagePresent = 0x001;
constexpr std::uint64_t kPageRw = 0x002;
constexpr std::uint64_t kPageUser = 0x004;
constexpr std::uint64_t kPageLarge = 0x080;
constexpr std::uint64_t kPageNx = 0x8000000000000000ULL;
constexpr std::uint64_t kPageAddrMask = 0x000FFFFFFFFFF000ULL;
constexpr std::size_t kPageSize4k = 4096;

struct PageIndices {
  std::uint64_t pml4 = 0;
  std::uint64_t pdpt = 0;
  std::uint64_t pd = 0;
  std::uint64_t pt = 0;
  std::uint64_t offset = 0;
};

/// Split a canonical virtual address into 4-level page-table indices.
PageIndices virt_to_indices(std::uint64_t virt_addr);

inline bool pte_present(std::uint64_t entry) {
  return (entry & kPagePresent) != 0;
}
inline bool pte_large(std::uint64_t entry) {
  return (entry & kPageLarge) != 0;
}
inline std::uint64_t pte_frame(std::uint64_t entry) {
  return entry & kPageAddrMask;
}

/// Physical address of a page-table entry given table base + index.
inline std::uint64_t pte_entry_phys(std::uint64_t table_phys, std::uint64_t index) {
  return table_phys + index * 8;
}

/// Resolve a 4K / 2M / 1G final physical address from a leaf PTE + VA.
/// Returns 0 if the entry is not present.
std::uint64_t resolve_leaf_phys(std::uint64_t leaf_pte, std::uint64_t virt_addr,
                                bool is_1g, bool is_2m);

/// Maximum bytes readable from `virt_addr` without crossing the leaf page.
std::size_t leaf_chunk_size(std::uint64_t virt_addr, std::size_t remaining,
                            bool is_1g, bool is_2m);

/// Validate a candidate DirectoryTableBase (page-aligned, non-zero, not all-ones).
bool is_plausible_cr3(std::uint64_t cr3);

/// Win10 / Win11 EPROCESS field offsets used by DKOM / CR3 scan paths.
struct EprocessLayout {
  std::uint64_t directory_table_base = 0x28;
  std::uint64_t unique_process_id = 0x2E0;
  std::uint64_t active_process_links = 0x2F0;
  std::uint64_t token = 0x358;
};

/// Select layout from which PID offset matched during a physical scan.
EprocessLayout eprocess_layout_for_pid_offset(std::uint64_t pid_field_offset);

}  // namespace real::kernel::mem
