// page_table_walker.cpp — x86-64 page table walk via DMA reads.
//
// Translates virtual addresses to physical addresses by walking 4-level
// page tables using physical memory reads (via PCIe DMA).
// No OS calls, no kernel driver, no process handle needed.
//
// Reference: Intel SDM Vol 3A, Chapter 4: Paging
//            Periscope prototype/src/verif/verif_memory_image.cpp

#include "fpga_dma.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

// ═══════════════════════════════════════════════════════════════════════
// x86-64 4-level page table walk
// ═══════════════════════════════════════════════════════════════════════

uint64_t PageTableWalker::translate(
    uint64_t virt_addr, uint64_t cr3,
    PhysReadFn phys_read_fn) noexcept
{
    if (!phys_read_fn) return 0;
    // Reject non-canonical canonical-form check for bits 63:48 (sign-extend of bit 47).
    // User/kernel half must match; non-canonical VAs are never present in real x86-64.
    const uint64_t top = virt_addr >> 47;
    if (top != 0 && top != 0x1FFFFULL) return 0;

    // Extract page table indices from virtual address
    const uint64_t pml4_idx = (virt_addr >> PML4_SHIFT) & 0x1FF;
    const uint64_t pdp_idx  = (virt_addr >> PDP_SHIFT)  & 0x1FF;
    const uint64_t pd_idx   = (virt_addr >> PD_SHIFT)   & 0x1FF;
    const uint64_t pt_idx   = (virt_addr >> PT_SHIFT)   & 0x1FF;
    const uint64_t offset   = virt_addr & (PAGE_SIZE_4K - 1);

    // ---- Step 1: PML4 ----
    uint64_t pml4e = 0;
    const uint64_t pml4_pa = (cr3 & PAGE_MASK) + pml4_idx * 8;
    if (!phys_read_fn(pml4_pa, &pml4e, sizeof(pml4e))) return 0;
    if (!(pml4e & PAGE_PRESENT)) return 0;

    // ---- Step 2: PDP (Page Directory Pointer) ----
    uint64_t pdpe = 0;
    const uint64_t pdp_pa = (pml4e & PAGE_MASK) + pdp_idx * 8;
    if (!phys_read_fn(pdp_pa, &pdpe, sizeof(pdpe))) return 0;
    if (!(pdpe & PAGE_PRESENT)) return 0;

    // 1GB page (PS bit set in PDPE) — frame in bits 51:30
    if (pdpe & PAGE_PS) {
        return (pdpe & ONE_GB_MASK) + (virt_addr & (PAGE_SIZE_1G - 1));
    }

    // ---- Step 3: PD (Page Directory) ----
    uint64_t pde = 0;
    const uint64_t pd_pa = (pdpe & PAGE_MASK) + pd_idx * 8;
    if (!phys_read_fn(pd_pa, &pde, sizeof(pde))) return 0;
    if (!(pde & PAGE_PRESENT)) return 0;

    // 2MB page (PS bit set in PDE) — frame in bits 51:21
    if (pde & PAGE_PS) {
        return (pde & LARGE_PAGE_MASK) + (virt_addr & (PAGE_SIZE_2M - 1));
    }

    // ---- Step 4: PT (Page Table) ----
    uint64_t pte = 0;
    const uint64_t pt_pa = (pde & PAGE_MASK) + pt_idx * 8;
    if (!phys_read_fn(pt_pa, &pte, sizeof(pte))) return 0;
    if (!(pte & PAGE_PRESENT)) return 0;

    // ---- Result: physical address (4KB frame bits 51:12 + offset) ----
    return (pte & PAGE_MASK) + offset;
}

// ═══════════════════════════════════════════════════════════════════════
// Read a buffer across page boundaries using VA → PA translation
// ═══════════════════════════════════════════════════════════════════════

bool PageTableWalker::read_virtual(
    uint64_t virt_addr, void* buf, size_t size,
    uint64_t cr3, PhysReadFn phys_read_fn) noexcept
{
    if (!phys_read_fn || !buf || size == 0) return false;

    uint8_t* dst = static_cast<uint8_t*>(buf);
    size_t done = 0;
    while (done < size) {
        const uint64_t pa = translate(virt_addr + done, cr3, phys_read_fn);
        // translate() returns 0 for not-present / error. Synthetic images never
        // map anything to physical page 0, so 0 always means failure here.
        if (!pa) return false;

        size_t chunk = static_cast<size_t>(PAGE_SIZE_4K) -
                       (static_cast<size_t>(pa) & static_cast<size_t>(PAGE_SIZE_4K - 1));
        if (chunk > size - done) chunk = size - done;
        if (!phys_read_fn(pa, dst + done, chunk)) return false;
        done += chunk;
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// EPROCESS candidate validation
// ═══════════════════════════════════════════════════════════════════════

namespace {

// XOR-obfuscated EPROCESS field offsets (defeat static scanning).
constexpr uint64_t kEpKey            = 0xB16B00B5CAFEBABEULL;
constexpr size_t   kEpDirTableBase   = 0x28 ^ kEpKey;   // KPROCESS.DirectoryTableBase
constexpr size_t   kEpWin10ImageFile = 0x2B8 ^ kEpKey;  // EPROCESS.ImageFileName (Win10)
constexpr size_t   kEpWin10Pid       = 0x2E0 ^ kEpKey;  // EPROCESS.UniqueProcessId (Win10)
constexpr size_t   kEpWin11ImageFile = 0x3E0 ^ kEpKey;  // EPROCESS.ImageFileName (Win11)
constexpr size_t   kEpWin11Pid       = 0x408 ^ kEpKey;  // EPROCESS.UniqueProcessId (Win11)

// XOR-obfuscated process name search strings: "cs2" and "CS2".
constexpr unsigned char kCs2Enc[] = { 0x9C, 0x8C, 0xCD, 0 };   // "cs2" ^ 0xFF
constexpr unsigned char kCS2Enc[] = { 0xBC, 0xAC, 0xCD, 0 };   // "CS2" ^ 0xFF

// Returns true if the 15-char ImageFileName candidate contains the target name.
bool name_matches(const char* candidate, const char* cs2_pat, const char* cs2_upper) {
    if (!candidate) return false;
    // Match either case variant as a substring ("cs2", "CS2", "Cs2").
    return strstr(candidate, cs2_pat) || strstr(candidate, cs2_upper);
}

// Validates that `cr3` plausibly points at a PML4 table: page-aligned,
// non-zero, and the first entry reads as a present entry.
bool cr3_looks_valid(PhysReadFn fn, uint64_t cr3) {
    if (!cr3 || (cr3 & 0xFFF) != 0) return false;
    uint64_t pml4e = 0;
    if (!fn(cr3, &pml4e, sizeof(pml4e))) return false;
    return (pml4e & PageTableWalker::PAGE_PRESENT) != 0;
}

}  // namespace

// ═══════════════════════════════════════════════════════════════════════
// Find process CR3 by scanning physical memory for EPROCESS
// ═══════════════════════════════════════════════════════════════════════

uint64_t PageTableWalker::find_eprocess(
    uint32_t target_pid, PhysReadFn phys_read_fn) noexcept
{
    if (!phys_read_fn) return 0;

    // ── EPROCESS cache: scan once, cache forever ──
    static uint32_t s_cached_pid = 0;
    static uint64_t s_cached_ep  = 0;
    static bool s_cached_valid   = false;
    if (s_cached_valid && s_cached_pid == target_pid) {
        return s_cached_ep;
    }

    // ── Decode the obfuscated process name patterns ──
    char cs2_pat[4], cs2_upper[4];
    for (int i = 0; i < 3; ++i) {
        cs2_pat[i]    = static_cast<char>(static_cast<uint8_t>(kCs2Enc[i])   ^ 0xFF);
        cs2_upper[i]  = static_cast<char>(static_cast<uint8_t>(kCS2Enc[i])   ^ 0xFF);
    }
    cs2_pat[3] = cs2_upper[3] = '\0';

    // ── Bounded scan regions (physical addresses, ascending) ──
    // EPROCESS blocks live in kernel pool / PagedPool; the physically-mapped
    // low region is the standard first target. Each region is [start, end).
    constexpr uint64_t kScanRegions[][2] = {
        { 0x00100000, 0x02000000 },   // Low kernel heap (common on Win10/11)
        { 0x10000000, 0x14000000 },   // Extended kernel pool (fallback)
        { 0x80000000, 0x90000000 },   // Session/registry space (rare)
    };

    uint8_t page[4096];

    for (auto region : kScanRegions) {
        uint64_t start = region[0], end = region[1];
        if (end <= start) continue;

        for (uint64_t pa = start; pa < end; pa += 0x1000) {
            if (!phys_read_fn(pa, page, sizeof(page)))
                continue;  // hole in physical memory map — skip

            // EPROCESS is page-aligned; check ImageFileName at both layouts.
            const char* name10 = reinterpret_cast<const char*>(page + (kEpWin10ImageFile ^ kEpKey));
            const char* name11 = reinterpret_cast<const char*>(page + (kEpWin11ImageFile ^ kEpKey));

            bool hit10 = name_matches(name10, cs2_pat, cs2_upper);
            bool hit11 = name_matches(name11, cs2_pat, cs2_upper);
            if (!hit10 && !hit11) continue;

            // Verify PID at the matching layout's offset.
            uint32_t pid10 = 0, pid11 = 0;
            std::memcpy(&pid10, page + (kEpWin10Pid ^ kEpKey), sizeof(pid10));
            std::memcpy(&pid11, page + (kEpWin11Pid ^ kEpKey), sizeof(pid11));
            if (hit10 && pid10 != target_pid && hit11 && pid11 != target_pid)
                continue;
            if (hit10 && pid10 != target_pid && !hit11) continue;
            if (hit11 && pid11 != target_pid && !hit10) continue;

            // Validate the DirectoryTableBase candidate.
            uint64_t cr3 = 0;
            std::memcpy(&cr3, page + (kEpDirTableBase ^ kEpKey), sizeof(cr3));
            if (!cr3_looks_valid(phys_read_fn, cr3)) continue;

            s_cached_pid = target_pid;
            s_cached_ep  = pa;
            s_cached_valid = true;
            return pa;
        }
    }
    return 0;
}

uint64_t PageTableWalker::find_process_cr3(
    uint32_t target_pid, PhysReadFn phys_read_fn) noexcept
{
    uint64_t ep = find_eprocess(target_pid, phys_read_fn);
    if (!ep) return 0;

    uint64_t cr3 = 0;
    if (!phys_read_fn(ep + (kEpDirTableBase ^ kEpKey), &cr3, sizeof(cr3)))
        return 0;
    return (cr3 & PAGE_MASK) != 0 ? cr3 : 0;
}
