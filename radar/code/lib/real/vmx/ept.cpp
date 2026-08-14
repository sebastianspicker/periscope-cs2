// Extended page table construction, encode helpers, and software GPA→HPA walk.
// EPT page-table entries store host PHYSICAL addresses. When the OS cannot
// expose PA (typical Windows usermode), a research synthetic-PA registry keeps
// virt↔phys consistent so hierarchy build/teardown and translate remain correct.

#include "real/vmx/vmx_intrin.hpp"
#include "real/vmx/vmx_priv.hpp"

#if LR_ARCH_X64

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#endif

#if LR_COMPILER_MSVC
#  include <malloc.h>
#endif

namespace real::vmx {
namespace {

constexpr uint64_t kPageSize = 4096;
constexpr uint64_t kLargePageSize = 0x200000;  // 2 MiB
constexpr uint64_t kPageMask = 0x000FFFFFFFFFF000ULL;
constexpr uint64_t kLargePageMask = 0x000FFFFFFFE00000ULL;
constexpr uint64_t kEptRwX = 0x7;
constexpr uint64_t kEptLarge = 1ULL << 7;
constexpr uint64_t kEptMemTypeShift = 3;

struct EptPage {
  uint64_t virt = 0;
  uint64_t phys = 0;
};

struct EptRegistry {
  std::mutex mu;
  std::unordered_map<uint64_t, uint64_t> phys_to_virt;
  std::unordered_map<uint64_t, uint64_t> virt_to_phys;
  // High research PA space — distinct from low real PAs when mixed.
  uint64_t next_synth_pa = 0x0000001000000000ULL;
};

EptRegistry& registry() {
  static EptRegistry r;
  return r;
}

void register_page(uint64_t virt, uint64_t phys) {
  auto& reg = registry();
  std::lock_guard<std::mutex> lock(reg.mu);
  reg.phys_to_virt[phys] = virt;
  reg.virt_to_phys[virt] = phys;
}

void unregister_page(uint64_t virt, uint64_t phys) {
  auto& reg = registry();
  std::lock_guard<std::mutex> lock(reg.mu);
  reg.phys_to_virt.erase(phys);
  reg.virt_to_phys.erase(virt);
}

uint64_t virt_of_phys(uint64_t phys) {
  auto& reg = registry();
  std::lock_guard<std::mutex> lock(reg.mu);
  auto it = reg.phys_to_virt.find(phys);
  if (it == reg.phys_to_virt.end()) return 0;
  return it->second;
}

EptPage allocate_ept_page() {
  EptPage page{};
#if LR_COMPILER_MSVC
  void* p = _aligned_malloc(kPageSize, kPageSize);
  if (p == nullptr) return {};
  page.virt = reinterpret_cast<uint64_t>(p);
#else
  void* p = nullptr;
  if (posix_memalign(&p, kPageSize, kPageSize) != 0 || p == nullptr) return {};
  page.virt = reinterpret_cast<uint64_t>(p);
#endif
  std::memset(reinterpret_cast<void*>(page.virt), 0, kPageSize);

  auto phys_result = virt_to_phys(reinterpret_cast<void*>(page.virt));
  if (phys_result) {
    page.phys = *phys_result;
  } else {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mu);
    page.phys = reg.next_synth_pa;
    reg.next_synth_pa += kPageSize;
  }
  register_page(page.virt, page.phys);
  return page;
}

void free_ept_page(EptPage page) {
  if (page.virt == 0) return;
  unregister_page(page.virt, page.phys);
#if LR_COMPILER_MSVC
  _aligned_free(reinterpret_cast<void*>(page.virt));
#else
  std::free(reinterpret_cast<void*>(page.virt));
#endif
}

uint64_t* table_at_phys(uint64_t phys) {
  uint64_t v = virt_of_phys(phys);
  if (v == 0) return nullptr;
  return reinterpret_cast<uint64_t*>(v);
}

void free_subtree_from_pml4(uint64_t* pml4) {
  for (size_t i = 0; i < 512; ++i) {
    if ((pml4[i] & 1) == 0) continue;
    const uint64_t pdpt_phys = pml4[i] & kPageMask;
    uint64_t* pdpt = table_at_phys(pdpt_phys);
    if (pdpt == nullptr) continue;
    for (size_t j = 0; j < 512; ++j) {
      if ((pdpt[j] & 1) == 0) continue;
      if ((pdpt[j] & kEptLarge) != 0) continue;  // 1 GiB leaf (rare)
      const uint64_t pd_phys = pdpt[j] & kPageMask;
      uint64_t* pd = table_at_phys(pd_phys);
      if (pd == nullptr) continue;
      for (size_t k = 0; k < 512; ++k) {
        if ((pd[k] & 1) == 0) continue;
        if ((pd[k] & kEptLarge) != 0) continue;  // 2 MiB leaf — no PT
        const uint64_t pt_phys = pd[k] & kPageMask;
        uint64_t pt_virt = virt_of_phys(pt_phys);
        if (pt_virt != 0) free_ept_page(EptPage{pt_virt, pt_phys});
      }
      free_ept_page(EptPage{reinterpret_cast<uint64_t>(pd), pd_phys});
    }
    free_ept_page(EptPage{reinterpret_cast<uint64_t>(pdpt), pdpt_phys});
  }
  // PML4 itself freed by caller with known phys.
}

}  // namespace

uint8_t EptPointer::memory_type() const {
  return static_cast<uint8_t>(value & 0x7ULL);
}

int EptPointer::page_walk_length() const {
  return static_cast<int>(((value >> 3) & 0x7ULL) + 1);
}

bool EptPointer::accessed_dirty() const {
  return (value & (1ULL << 6)) != 0;
}

uint64_t EptPointer::pml4() const {
  return value & kPageMask;
}

void ept_entry_set_phys_addr(uint64_t& entry, uint64_t phys_addr) {
  entry = (entry & ~kPageMask) | (phys_addr & kPageMask);
}

uint64_t ept_entry_get_phys_addr(uint64_t entry) {
  return entry & kPageMask;
}

void ept_entry_set_access(uint64_t& entry, bool read, bool write, bool execute) {
  entry = (entry & ~0x7ULL) | (read ? 1ULL : 0) | (write ? 2ULL : 0) | (execute ? 4ULL : 0);
}

void ept_entry_get_access(uint64_t entry, bool& read, bool& write, bool& execute) {
  read = (entry & 1ULL) != 0;
  write = (entry & 2ULL) != 0;
  execute = (entry & 4ULL) != 0;
}

void ept_entry_set_memory_type(uint64_t& entry, uint8_t memory_type) {
  entry = (entry & ~(0x7ULL << kEptMemTypeShift)) |
          ((static_cast<uint64_t>(memory_type) & 0x7ULL) << kEptMemTypeShift);
}

uint8_t ept_entry_get_memory_type(uint64_t entry) {
  return static_cast<uint8_t>((entry >> kEptMemTypeShift) & 0x7ULL);
}

bool ept_entry_is_large(uint64_t entry) {
  return (entry & kEptLarge) != 0;
}

Result<EptPointer> build_ept_hierarchy_ex(uint64_t guest_phys_base, size_t size,
                                          uint64_t host_phys_base,
                                          const EptBuildOptions& opts) {
  if (size == 0) return Result<EptPointer>({}, "EPT mapping size must be non-zero");
  if (((guest_phys_base | host_phys_base) & (kPageSize - 1)) != 0) {
    return Result<EptPointer>({}, "EPT base addresses must be 4KB aligned");
  }
  const uint64_t mapping_size =
      (static_cast<uint64_t>(size) + kPageSize - 1) & ~(kPageSize - 1);
  const uint8_t mem_type = opts.write_back ? 6 : 0;

  EptPage pml4_page = allocate_ept_page();
  if (pml4_page.virt == 0) return Result<EptPointer>({}, "EPT PML4 allocation failed");
  auto* pml4 = reinterpret_cast<uint64_t*>(pml4_page.virt);

  std::vector<EptPage> allocated;
  allocated.push_back(pml4_page);

  auto fail_cleanup = [&]() -> Result<EptPointer> {
    for (auto it = allocated.rbegin(); it != allocated.rend(); ++it) {
      free_ept_page(*it);
    }
    return Result<EptPointer>({}, "EPT table allocation failed");
  };

  for (uint64_t offset = 0; offset < mapping_size;) {
    const uint64_t guest = guest_phys_base + offset;
    const size_t pml4_index = (guest >> 39) & 0x1ff;
    const size_t pdpt_index = (guest >> 30) & 0x1ff;
    const size_t pd_index = (guest >> 21) & 0x1ff;

    // PDPT
    if ((pml4[pml4_index] & 1) == 0) {
      EptPage p = allocate_ept_page();
      if (p.virt == 0) return fail_cleanup();
      pml4[pml4_index] = p.phys | kEptRwX;
      allocated.push_back(p);
    }
    uint64_t* pdpt = table_at_phys(pml4[pml4_index] & kPageMask);
    if (pdpt == nullptr) return fail_cleanup();

    // PD
    if ((pdpt[pdpt_index] & 1) == 0) {
      EptPage p = allocate_ept_page();
      if (p.virt == 0) return fail_cleanup();
      pdpt[pdpt_index] = p.phys | kEptRwX;
      allocated.push_back(p);
    }
    uint64_t* pd = table_at_phys(pdpt[pdpt_index] & kPageMask);
    if (pd == nullptr) return fail_cleanup();

    const bool use_2mb = opts.use_large_pages &&
                         (guest & (kLargePageSize - 1)) == 0 &&
                         ((host_phys_base + offset) & (kLargePageSize - 1)) == 0 &&
                         (offset + kLargePageSize) <= mapping_size;

    if (use_2mb) {
      // 2 MiB leaf: PS bit set, HPA in bits 51:21.
      uint64_t entry = 0;
      ept_entry_set_phys_addr(entry, (host_phys_base + offset) & kLargePageMask);
      ept_entry_set_access(entry, true, true, true);
      ept_entry_set_memory_type(entry, mem_type);
      entry |= kEptLarge;
      pd[pd_index] = entry;
      offset += kLargePageSize;
    } else {
      const size_t pt_index = (guest >> 12) & 0x1ff;
      if ((pd[pd_index] & 1) == 0) {
        EptPage p = allocate_ept_page();
        if (p.virt == 0) return fail_cleanup();
        pd[pd_index] = p.phys | kEptRwX;
        allocated.push_back(p);
      } else if ((pd[pd_index] & kEptLarge) != 0) {
        return fail_cleanup();  // conflict with prior large mapping
      }
      uint64_t* pt = table_at_phys(pd[pd_index] & kPageMask);
      if (pt == nullptr) return fail_cleanup();

      uint64_t entry = 0;
      ept_entry_set_phys_addr(entry, host_phys_base + offset);
      ept_entry_set_access(entry, true, true, true);
      ept_entry_set_memory_type(entry, mem_type);
      pt[pt_index] = entry;
      offset += kPageSize;
    }
  }

  EptPointer eptp;
  // EPTP: memtype | (walk_length-1)<<3 | PML4 phys
  eptp.value = (pml4_page.phys & kPageMask) | static_cast<uint64_t>(mem_type) | (3ULL << 3);
  eptp.pml4_virtual = reinterpret_cast<void*>(pml4_page.virt);
  // Ownership transferred to caller via destroy_ept_hierarchy; do not free here.
  (void)allocated;
  return Result<EptPointer>(eptp);
}

Result<EptPointer> build_ept_hierarchy(uint64_t guest_phys_base, size_t size,
                                       uint64_t host_phys_base) {
  EptBuildOptions opts;
  return build_ept_hierarchy_ex(guest_phys_base, size, host_phys_base, opts);
}

Result<uint64_t> ept_translate_gpa(const EptPointer& eptp, uint64_t gpa) {
  if (eptp.pml4_virtual == nullptr && eptp.pml4() == 0) {
    return Result<uint64_t>(0, "EPT: empty hierarchy");
  }
  uint64_t* pml4 = eptp.pml4_virtual
                       ? reinterpret_cast<uint64_t*>(const_cast<void*>(eptp.pml4_virtual))
                       : table_at_phys(eptp.pml4());
  if (pml4 == nullptr) return Result<uint64_t>(0, "EPT: PML4 not mapped in registry");

  const size_t pml4_index = (gpa >> 39) & 0x1ff;
  if ((pml4[pml4_index] & 1) == 0) return Result<uint64_t>(0, "EPT: PML4E not present");

  uint64_t* pdpt = table_at_phys(pml4[pml4_index] & kPageMask);
  if (pdpt == nullptr) return Result<uint64_t>(0, "EPT: PDPT missing");
  const size_t pdpt_index = (gpa >> 30) & 0x1ff;
  if ((pdpt[pdpt_index] & 1) == 0) return Result<uint64_t>(0, "EPT: PDPTE not present");
  if ((pdpt[pdpt_index] & kEptLarge) != 0) {
    // 1 GiB page
    return Result<uint64_t>((pdpt[pdpt_index] & 0x000FFFFFC0000000ULL) | (gpa & 0x3FFFFFFFULL));
  }

  uint64_t* pd = table_at_phys(pdpt[pdpt_index] & kPageMask);
  if (pd == nullptr) return Result<uint64_t>(0, "EPT: PD missing");
  const size_t pd_index = (gpa >> 21) & 0x1ff;
  if ((pd[pd_index] & 1) == 0) return Result<uint64_t>(0, "EPT: PDE not present");
  if ((pd[pd_index] & kEptLarge) != 0) {
    // 2 MiB page
    return Result<uint64_t>((pd[pd_index] & kLargePageMask) | (gpa & (kLargePageSize - 1)));
  }

  uint64_t* pt = table_at_phys(pd[pd_index] & kPageMask);
  if (pt == nullptr) return Result<uint64_t>(0, "EPT: PT missing");
  const size_t pt_index = (gpa >> 12) & 0x1ff;
  if ((pt[pt_index] & 1) == 0) return Result<uint64_t>(0, "EPT: PTE not present");
  return Result<uint64_t>((pt[pt_index] & kPageMask) | (gpa & (kPageSize - 1)));
}

Result<void> destroy_ept_hierarchy(const EptPointer& eptp, const void* pml4_virtual) {
  const void* pml4_v = pml4_virtual != nullptr ? pml4_virtual : eptp.pml4_virtual;
  if (pml4_v == nullptr) return Result<void>();

  auto* pml4 = const_cast<uint64_t*>(reinterpret_cast<const uint64_t*>(pml4_v));
  free_subtree_from_pml4(pml4);

  uint64_t pml4_phys = eptp.pml4();
  if (pml4_phys == 0) {
    auto& reg = registry();
    std::lock_guard<std::mutex> lock(reg.mu);
    auto it = reg.virt_to_phys.find(reinterpret_cast<uint64_t>(pml4_v));
    if (it != reg.virt_to_phys.end()) pml4_phys = it->second;
  }
  free_ept_page(EptPage{reinterpret_cast<uint64_t>(pml4_v), pml4_phys});

  if (eptp.value != 0) {
    // Best-effort TLB invalidation; structured error ignored in usermode.
    (void)invept(1, eptp.value);
  }
  return Result<void>();
}

Result<std::vector<uint8_t>> vmx_read_physical(uint64_t phys_addr, size_t size) {
  if (size == 0) return Result<std::vector<uint8_t>>(std::vector<uint8_t>{});

  // Research path: if PA is in our EPT registry, read via virtual mapping.
  {
    const uint64_t page_base = phys_addr & kPageMask;
    const uint64_t page_virt = virt_of_phys(page_base);
    if (page_virt != 0) {
      const size_t off = static_cast<size_t>(phys_addr - page_base);
      if (off + size <= kPageSize) {
        std::vector<uint8_t> out(size);
        std::memcpy(out.data(), reinterpret_cast<const uint8_t*>(page_virt) + off, size);
        return Result<std::vector<uint8_t>>(std::move(out));
      }
    }
  }

#if LR_PLATFORM_LINUX
  const int fd = open("/dev/mem", O_RDONLY | O_SYNC);
  if (fd < 0) {
    return Result<std::vector<uint8_t>>({}, "open /dev/mem failed (need privileges or research PA)");
  }
  const uint64_t page_start = phys_addr & ~(kPageSize - 1);
  const size_t offset = static_cast<size_t>(phys_addr - page_start);
  const size_t map_size = size + offset;
  void* map = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, static_cast<off_t>(page_start));
  if (map == MAP_FAILED) {
    close(fd);
    return Result<std::vector<uint8_t>>({}, "mmap /dev/mem failed");
  }
  std::vector<uint8_t> result(size);
  std::memcpy(result.data(), static_cast<uint8_t*>(map) + offset, size);
  munmap(map, map_size);
  close(fd);
  return Result<std::vector<uint8_t>>(std::move(result));
#else
  (void)phys_addr;
  return Result<std::vector<uint8_t>>(
      {}, "Physical memory read unsupported without research mapping or /dev/mem");
#endif
}

}  // namespace real::vmx
#endif  // LR_ARCH_X64
