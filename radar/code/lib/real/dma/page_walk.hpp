// page_walk.hpp — Pure x86-64 page-table walk + scatter-gather helpers.
//
// These units are deliberately free of OS I/O so they can be unit-tested
// against synthetic physical memory (injected PhysReadFn). Platform DMA
// transports (physmem, FPGA, BAR) supply the physical-read callback.

#pragma once

#include "real/error.hpp"

#include <cstdint>
#include <cstddef>
#include <vector>

namespace real::dma {

// Physical-memory read callback used by the page walker.
// Return true on success (exactly `size` bytes written to `buf`).
// `ctx` is an opaque user pointer (e.g. a synthetic memory map for tests).
using PhysReadFn = bool (*)(std::uint64_t phys_addr, void* buf, std::size_t size,
                            void* ctx);

// Result of a single VA→PA translation.
struct PageWalkResult {
  std::uint64_t phys_addr = 0;  // full physical address (page base + offset)
  std::uint64_t page_size = 0;  // 4 KiB, 2 MiB, or 1 GiB when present
  bool present = false;
};

// x86-64 paging constants (Intel SDM Vol 3A Ch. 4).
struct PageTableConstants {
  static constexpr int kPml4Shift = 39;
  static constexpr int kPdpShift = 30;
  static constexpr int kPdShift = 21;
  static constexpr int kPtShift = 12;
  static constexpr std::uint64_t kPageMask = 0x0000FFFFFFFFF000ULL;
  static constexpr std::uint64_t kLargePageMask = 0x0000FFFFFFFE0000ULL;  // 2 MiB
  static constexpr std::uint64_t kOneGbMask = 0x0000FFFFC0000000ULL;      // 1 GiB
  static constexpr std::uint64_t kPresent = 0x001ULL;
  static constexpr std::uint64_t kPageSizeBit = 0x080ULL;  // PS in PDPE/PDE
  static constexpr std::uint64_t kPage4k = 0x1000ULL;
  static constexpr std::uint64_t kPage2m = 0x200000ULL;
  static constexpr std::uint64_t kPage1g = 0x40000000ULL;
};

// Translate a virtual address through a 4-level x86-64 page table rooted at
// `cr3` using `read_fn` for every physical table/page read.
// Returns present=false (phys_addr=0) for unmapped entries, null callback,
// or any physical-read failure.
PageWalkResult translate_va(std::uint64_t virt_addr, std::uint64_t cr3,
                            PhysReadFn read_fn, void* ctx = nullptr);

// Convenience: returns physical address or 0 when not present.
std::uint64_t translate_va_or_zero(std::uint64_t virt_addr, std::uint64_t cr3,
                                   PhysReadFn read_fn, void* ctx = nullptr);

// Read `size` bytes at virtual address `virt_addr` by walking page tables
// and issuing one physical read per page-aligned chunk. Handles multi-page
// spans (including 4K/2M/1G page sizes). Returns false on any unmapped
// page or physical-read failure.
bool read_virtual(std::uint64_t virt_addr, void* buf, std::size_t size,
                  std::uint64_t cr3, PhysReadFn read_fn, void* ctx = nullptr);

// Result wrapper for virtual multi-page reads (matches other DMA APIs).
Result<std::vector<std::uint8_t>> read_virtual_bytes(
    std::uint64_t virt_addr, std::size_t size, std::uint64_t cr3,
    PhysReadFn read_fn, void* ctx = nullptr);

// ── Scatter-gather descriptor packing (FPGA / PCILeech pattern) ────

// One DMA transfer descriptor. Pure host-side construction; the FPGA
// transport writes these into the device descriptor ring.
struct ScatterDescriptor {
  std::uint64_t src_phys = 0;    // physical source address
  std::uint64_t dst_offset = 0;  // offset into result buffer / FPGA SRAM
  std::uint32_t length = 0;      // transfer length in bytes
  std::uint32_t flags = 0;       // bit0 = valid/read
};

// Build page-aligned scatter descriptors covering [phys_addr, phys_addr+size).
// The first and last descriptors may be partial pages; middle ones are full.
// `page_size` defaults to 4 KiB (standard DMA page).
// Empty when size==0. Never throws.
std::vector<ScatterDescriptor> build_scatter_list(std::uint64_t phys_addr,
                                                  std::size_t size,
                                                  std::size_t page_size = 4096);

// Split a virtual multi-page read into ordered (phys, len) chunks using
// translate_va for each page. Fails (empty + error) if any page is unmapped.
Result<std::vector<ScatterDescriptor>> build_virtual_scatter(
    std::uint64_t virt_addr, std::size_t size, std::uint64_t cr3,
    PhysReadFn read_fn, void* ctx = nullptr,
    std::size_t page_size = 4096);

}  // namespace real::dma
