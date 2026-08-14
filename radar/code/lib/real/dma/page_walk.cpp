// page_walk.cpp — x86-64 multi-level page-table walk + scatter construction.
//
// Reference: Intel SDM Vol 3A Chapter 4 (Paging).
// Lifted into lib/real/dma so pure logic is unit-testable without FPGA
// firmware and without re-implementing an oracle in the test binary.

#include "real/dma/page_walk.hpp"

#include <algorithm>
#include <cstring>

namespace real::dma {

namespace {

bool phys_read_u64(PhysReadFn fn, void* ctx, std::uint64_t pa, std::uint64_t& out) {
  if (!fn) return false;
  return fn(pa, &out, sizeof(out), ctx);
}

}  // namespace

PageWalkResult translate_va(std::uint64_t virt_addr, std::uint64_t cr3,
                            PhysReadFn read_fn, void* ctx) {
  PageWalkResult result{};
  if (!read_fn) return result;

  using C = PageTableConstants;

  const std::uint64_t pml4_idx = (virt_addr >> C::kPml4Shift) & 0x1FF;
  const std::uint64_t pdp_idx = (virt_addr >> C::kPdpShift) & 0x1FF;
  const std::uint64_t pd_idx = (virt_addr >> C::kPdShift) & 0x1FF;
  const std::uint64_t pt_idx = (virt_addr >> C::kPtShift) & 0x1FF;

  // ---- PML4 ----
  std::uint64_t pml4e = 0;
  const std::uint64_t pml4_pa = (cr3 & C::kPageMask) + pml4_idx * 8;
  if (!phys_read_u64(read_fn, ctx, pml4_pa, pml4e)) return result;
  if ((pml4e & C::kPresent) == 0) return result;

  // ---- PDP ----
  std::uint64_t pdpe = 0;
  const std::uint64_t pdp_pa = (pml4e & C::kPageMask) + pdp_idx * 8;
  if (!phys_read_u64(read_fn, ctx, pdp_pa, pdpe)) return result;
  if ((pdpe & C::kPresent) == 0) return result;

  // 1 GiB page (PS bit in PDPE)
  if (pdpe & C::kPageSizeBit) {
    const std::uint64_t offset = virt_addr & (C::kPage1g - 1);
    result.phys_addr = (pdpe & C::kOneGbMask) + offset;
    result.page_size = C::kPage1g;
    result.present = true;
    return result;
  }

  // ---- PD ----
  std::uint64_t pde = 0;
  const std::uint64_t pd_pa = (pdpe & C::kPageMask) + pd_idx * 8;
  if (!phys_read_u64(read_fn, ctx, pd_pa, pde)) return result;
  if ((pde & C::kPresent) == 0) return result;

  // 2 MiB page (PS bit in PDE)
  if (pde & C::kPageSizeBit) {
    const std::uint64_t offset = virt_addr & (C::kPage2m - 1);
    result.phys_addr = (pde & C::kLargePageMask) + offset;
    result.page_size = C::kPage2m;
    result.present = true;
    return result;
  }

  // ---- PT ----
  std::uint64_t pte = 0;
  const std::uint64_t pt_pa = (pde & C::kPageMask) + pt_idx * 8;
  if (!phys_read_u64(read_fn, ctx, pt_pa, pte)) return result;
  if ((pte & C::kPresent) == 0) return result;

  const std::uint64_t offset = virt_addr & (C::kPage4k - 1);
  result.phys_addr = (pte & C::kPageMask) + offset;
  result.page_size = C::kPage4k;
  result.present = true;
  return result;
}

std::uint64_t translate_va_or_zero(std::uint64_t virt_addr, std::uint64_t cr3,
                                   PhysReadFn read_fn, void* ctx) {
  const auto r = translate_va(virt_addr, cr3, read_fn, ctx);
  return r.present ? r.phys_addr : 0;
}

bool read_virtual(std::uint64_t virt_addr, void* buf, std::size_t size,
                  std::uint64_t cr3, PhysReadFn read_fn, void* ctx) {
  if (!read_fn || !buf) return false;
  if (size == 0) return true;

  auto* dst = static_cast<std::uint8_t*>(buf);
  std::size_t done = 0;
  while (done < size) {
    const auto walk = translate_va(virt_addr + done, cr3, read_fn, ctx);
    if (!walk.present) return false;

    // Remaining bytes inside the current physical page.
    const std::uint64_t page_off = walk.phys_addr & (walk.page_size - 1);
    std::size_t chunk = static_cast<std::size_t>(walk.page_size - page_off);
    if (chunk > size - done) chunk = size - done;

    if (!read_fn(walk.phys_addr, dst + done, chunk, ctx)) return false;
    done += chunk;
  }
  return true;
}

Result<std::vector<std::uint8_t>> read_virtual_bytes(
    std::uint64_t virt_addr, std::size_t size, std::uint64_t cr3,
    PhysReadFn read_fn, void* ctx) {
  if (size == 0) return std::vector<std::uint8_t>{};
  std::vector<std::uint8_t> out(size);
  if (!read_virtual(virt_addr, out.data(), size, cr3, read_fn, ctx)) {
    return Result<std::vector<std::uint8_t>>(
        {}, "read_virtual: unmapped page or physical read failed");
  }
  return out;
}

std::vector<ScatterDescriptor> build_scatter_list(std::uint64_t phys_addr,
                                                  std::size_t size,
                                                  std::size_t page_size) {
  std::vector<ScatterDescriptor> descs;
  if (size == 0 || page_size == 0) return descs;

  std::uint64_t addr = phys_addr;
  std::size_t remaining = size;
  std::uint64_t dst = 0;

  while (remaining > 0) {
    const std::size_t page_off =
        static_cast<std::size_t>(addr & (static_cast<std::uint64_t>(page_size) - 1));
    std::size_t chunk = page_size - page_off;
    if (chunk > remaining) chunk = remaining;

    ScatterDescriptor d;
    d.src_phys = addr;
    d.dst_offset = dst;
    d.length = static_cast<std::uint32_t>(chunk);
    d.flags = 0x1;  // valid / read
    descs.push_back(d);

    addr += chunk;
    dst += chunk;
    remaining -= chunk;
  }
  return descs;
}

Result<std::vector<ScatterDescriptor>> build_virtual_scatter(
    std::uint64_t virt_addr, std::size_t size, std::uint64_t cr3,
    PhysReadFn read_fn, void* ctx, std::size_t page_size) {
  std::vector<ScatterDescriptor> descs;
  if (size == 0) return descs;
  if (!read_fn) {
    return Result<std::vector<ScatterDescriptor>>({}, "null PhysReadFn");
  }
  if (page_size == 0) page_size = 4096;

  std::size_t done = 0;
  std::uint64_t dst = 0;
  while (done < size) {
    const auto walk = translate_va(virt_addr + done, cr3, read_fn, ctx);
    if (!walk.present) {
      return Result<std::vector<ScatterDescriptor>>(
          {}, "build_virtual_scatter: unmapped virtual page");
    }

    // Cap each descriptor at `page_size` for FPGA ring compatibility, even
    // when the translation landed on a 2 MiB / 1 GiB large page.
    const std::uint64_t large_off = walk.phys_addr & (walk.page_size - 1);
    std::size_t in_page =
        static_cast<std::size_t>(walk.page_size - large_off);
    if (in_page > size - done) in_page = size - done;

    // Further split into page_size chunks.
    std::size_t local = 0;
    while (local < in_page) {
      const std::uint64_t pa = walk.phys_addr + local;
      const std::size_t page_off =
          static_cast<std::size_t>(pa & (static_cast<std::uint64_t>(page_size) - 1));
      std::size_t chunk = page_size - page_off;
      if (chunk > in_page - local) chunk = in_page - local;

      ScatterDescriptor d;
      d.src_phys = pa;
      d.dst_offset = dst;
      d.length = static_cast<std::uint32_t>(chunk);
      d.flags = 0x1;
      descs.push_back(d);

      local += chunk;
      dst += chunk;
    }
    done += in_page;
  }
  return descs;
}

}  // namespace real::dma
