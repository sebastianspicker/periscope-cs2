// page_walk.cpp — Software x86-64 page-table walk via physical reads.

#include "real/linux/page_walk.hpp"

#include <cstring>

namespace real::linux::page_walk {

Result<uint64_t> read_pte(uint64_t phys_entry_addr,
                          const PhysReadFn& phys_read) noexcept {
    if (!phys_read) return Result<uint64_t>(0, "null phys_read");
    auto bytes = phys_read(phys_entry_addr, 8);
    if (!bytes) return Result<uint64_t>(0, bytes.error_msg);
    if (bytes->size() < 8) return Result<uint64_t>(0, "short PTE read");
    uint64_t entry = 0;
    std::memcpy(&entry, bytes->data(), 8);
    return entry;
}

Result<pagemap::WalkResult> walk(uint64_t cr3, uint64_t va,
                                 const PhysReadFn& phys_read) noexcept {
    pagemap::WalkResult fail;
    if (!phys_read) {
        fail.fail_stage = "phys_read";
        return Result<pagemap::WalkResult>(fail, "null phys_read");
    }

    const uint64_t pml4_phys = cr3 & pagemap::kPhysAddrMask;
    auto pml4e = read_pte(table_slot_phys(pml4_phys, pagemap::pml4_index(va)),
                          phys_read);
    if (!pml4e) {
        fail.fail_stage = "pml4e_read";
        return Result<pagemap::WalkResult>(fail, pml4e.error_msg);
    }
    if (!pagemap::pte_present(*pml4e)) {
        fail.pml4e = *pml4e;
        fail.fail_stage = "pml4e";
        return Result<pagemap::WalkResult>(fail, "PML4E not present");
    }

    auto pdpte = read_pte(
        table_slot_phys(pagemap::pte_phys_base(*pml4e), pagemap::pdpt_index(va)),
        phys_read);
    if (!pdpte) {
        fail.pml4e = *pml4e;
        fail.fail_stage = "pdpte_read";
        return Result<pagemap::WalkResult>(fail, pdpte.error_msg);
    }
    if (!pagemap::pte_present(*pdpte)) {
        fail.pml4e = *pml4e;
        fail.pdpte = *pdpte;
        fail.fail_stage = "pdpte";
        return Result<pagemap::WalkResult>(fail, "PDPTE not present");
    }
    if (pagemap::pte_large(*pdpte)) {
        auto r = pagemap::walk_from_entries(va, *pml4e, *pdpte, 0, 0);
        return r;
    }

    auto pde = read_pte(
        table_slot_phys(pagemap::pte_phys_base(*pdpte), pagemap::pd_index(va)),
        phys_read);
    if (!pde) {
        fail.pml4e = *pml4e;
        fail.pdpte = *pdpte;
        fail.fail_stage = "pde_read";
        return Result<pagemap::WalkResult>(fail, pde.error_msg);
    }
    if (!pagemap::pte_present(*pde)) {
        fail.pml4e = *pml4e;
        fail.pdpte = *pdpte;
        fail.pde = *pde;
        fail.fail_stage = "pde";
        return Result<pagemap::WalkResult>(fail, "PDE not present");
    }
    if (pagemap::pte_large(*pde)) {
        auto r = pagemap::walk_from_entries(va, *pml4e, *pdpte, *pde, 0);
        return r;
    }

    auto pte = read_pte(
        table_slot_phys(pagemap::pte_phys_base(*pde), pagemap::pt_index(va)),
        phys_read);
    if (!pte) {
        fail.pml4e = *pml4e;
        fail.pdpte = *pdpte;
        fail.pde = *pde;
        fail.fail_stage = "pte_read";
        return Result<pagemap::WalkResult>(fail, pte.error_msg);
    }

    auto r = pagemap::walk_from_entries(va, *pml4e, *pdpte, *pde, *pte);
    if (!r.ok) {
        return Result<pagemap::WalkResult>(r, r.fail_stage ? r.fail_stage : "walk");
    }
    return r;
}

} // namespace real::linux::page_walk
