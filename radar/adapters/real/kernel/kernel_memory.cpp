// kernel_memory.cpp — Kernel-level memory operations (educational).
// Every function documents the forensic scar, detection vector, and mitigation.
//
// LESSON: Kernel memory access is the T2/T3 privilege level. Operations here
// bypass ALL usermode anti-cheat sensors (handle enumeration, API hooks, etc.)
// but leave their OWN artifacts detectable by kernel-level AC sensors.

#include "real/kernel/kernel_memory.hpp"
#include "real/kernel/ioctl_interface.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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

// ── Pure helpers ───────────────────────────────────────────────────

PageIndices virt_to_indices(std::uint64_t virt_addr) {
  PageIndices idx;
  idx.pml4 = (virt_addr >> 39) & 0x1FF;
  idx.pdpt = (virt_addr >> 30) & 0x1FF;
  idx.pd = (virt_addr >> 21) & 0x1FF;
  idx.pt = (virt_addr >> 12) & 0x1FF;
  idx.offset = virt_addr & 0xFFF;
  return idx;
}

std::uint64_t resolve_leaf_phys(std::uint64_t leaf_pte, std::uint64_t virt_addr,
                                bool is_1g, bool is_2m) {
  if (!pte_present(leaf_pte)) return 0;
  const std::uint64_t frame = pte_frame(leaf_pte);
  if (is_1g) return frame + (virt_addr & 0x3FFFFFFFULL);
  if (is_2m) return frame + (virt_addr & 0x1FFFFFULL);
  return frame + (virt_addr & 0xFFFULL);
}

std::size_t leaf_chunk_size(std::uint64_t virt_addr, std::size_t remaining,
                            bool is_1g, bool is_2m) {
  std::uint64_t page_mask;
  if (is_1g) page_mask = 0x3FFFFFFFULL;
  else if (is_2m) page_mask = 0x1FFFFFULL;
  else page_mask = 0xFFFULL;
  const std::size_t page_rem =
      static_cast<std::size_t>((page_mask + 1) - (virt_addr & page_mask));
  return (std::min)(remaining, page_rem);
}

bool is_plausible_cr3(std::uint64_t cr3) {
  if (cr3 == 0) return false;
  if ((cr3 & 0xFFFULL) != 0) return false;
  if (cr3 >= 0xFFFFFFFFFFFFF000ULL) return false;
  return true;
}

EprocessLayout eprocess_layout_for_pid_offset(std::uint64_t pid_field_offset) {
  EprocessLayout layout;
  layout.directory_table_base = 0x28;
  if (pid_field_offset == 0x308) {
    // Win11-style
    layout.unique_process_id = 0x308;
    layout.active_process_links = 0x318;
    layout.token = 0x380;
  } else {
    // Win10 20H2+
    layout.unique_process_id = 0x2E0;
    layout.active_process_links = 0x2F0;
    layout.token = 0x358;
  }
  return layout;
}

// ── Windows physical access helpers ────────────────────────────────

#if LR_PLATFORM_WINDOWS
namespace {

Result<std::vector<std::uint8_t>> read_physical_via_section(std::uint64_t phys_addr,
                                                            std::size_t size) {
  auto& api = real::win::g_Api();

  // Path 1: \\.\PhysicalMemory (blocked on modern Windows for non-drivers).
  if (api.CreateFileA && api.ReadFile && api.CloseHandle) {
    HANDLE h = api.CreateFileA(OBF("\\\\.\\PhysicalMemory"), GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                               OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      LARGE_INTEGER offset;
      offset.QuadPart = static_cast<LONGLONG>(phys_addr);
      std::vector<std::uint8_t> buf(size);
      DWORD read = 0;
      if (SetFilePointerEx(h, offset, nullptr, FILE_BEGIN) &&
          api.ReadFile(h, buf.data(), static_cast<DWORD>(size), &read, nullptr) &&
          read > 0) {
        buf.resize(read);
        api.CloseHandle(h);
        return buf;
      }
      api.CloseHandle(h);
    }
  }

  // Path 2: NtOpenSection(\\Device\\PhysicalMemory) + NtMapViewOfSection.
  using NtOpenSectionFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
  static NtOpenSectionFn pNtOpenSection = nullptr;
  static bool resolved = false;
  if (!resolved) {
    resolved = true;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
      pNtOpenSection = reinterpret_cast<NtOpenSectionFn>(
          GetProcAddress(ntdll, "NtOpenSection"));
    }
  }

  if (pNtOpenSection && api.NtMapViewOfSection && api.NtUnmapViewOfSection &&
      api.NtClose) {
    UNICODE_STRING obj_name{};
    wchar_t name_buf[] = L"\\Device\\PhysicalMemory";
    obj_name.Buffer = name_buf;
    obj_name.Length = static_cast<USHORT>(wcslen(name_buf) * sizeof(wchar_t));
    obj_name.MaximumLength = obj_name.Length + sizeof(wchar_t);

    OBJECT_ATTRIBUTES obj_attr{};
    InitializeObjectAttributes(&obj_attr, &obj_name, OBJ_CASE_INSENSITIVE,
                               nullptr, nullptr);
    HANDLE section = nullptr;
    NTSTATUS status = pNtOpenSection(&section, SECTION_MAP_READ, &obj_attr);
    if (NT_SUCCESS(status) && section) {
      void* view = nullptr;
      SIZE_T view_size = size;
      LARGE_INTEGER section_offset;
      section_offset.QuadPart = static_cast<LONGLONG>(phys_addr);
      constexpr ULONG kViewShare = 1;
      status = api.NtMapViewOfSection(
          section, reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)), &view, 0,
          size, &section_offset, &view_size, kViewShare, 0, PAGE_READONLY);
      if (NT_SUCCESS(status) && view != nullptr) {
        std::vector<std::uint8_t> buf(size);
        std::memcpy(buf.data(), view, size);
        api.NtUnmapViewOfSection(
            reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)), view);
        api.NtClose(section);
        return buf;
      }
      api.NtClose(section);
    }
  }

  return Result<std::vector<std::uint8_t>>(
      {}, "PhysicalMemory section map denied (need driver or SeDebugPrivilege)");
}

Result<std::vector<std::uint8_t>> read_physical_via_gdrv(std::uint64_t phys_addr,
                                                         std::size_t size) {
  // Align with examples/drivers/example_vulnerable: IOCTL 0xC3502000 + GDRV_PHYS_REQ.
  constexpr std::uint32_t kPhysRead = 0xC3502000u;
  const char* candidates[] = {
      "\\\\.\\gdrv",
      "\\\\.\\GIO",
      "\\\\.\\WinRing0_1_2_0",
      "\\\\.\\EneIo",
      "\\\\.\\EneIo64",
  };

  struct PhysReq {
    std::uint64_t phys_addr;
    std::uint32_t size;
    std::uint32_t reserved;
  };

  for (const char* path : candidates) {
    auto dev = open_device(path);
    if (!dev) continue;

    const std::size_t chunk_cap = 512;  // educational driver caps at 512
    std::vector<std::uint8_t> total;
    total.reserve(size);
    std::size_t done = 0;
    bool ok = true;
    while (done < size) {
      const std::size_t chunk = (std::min)(size - done, chunk_cap);
      PhysReq req{phys_addr + done, static_cast<std::uint32_t>(chunk), 0};
      std::vector<std::uint8_t> in(sizeof(req));
      std::memcpy(in.data(), &req, sizeof(req));
      auto out = send_ioctl(*dev, kPhysRead, in, chunk);
      if (!out || out->size() < chunk) {
        ok = false;
        break;
      }
      total.insert(total.end(), out->begin(), out->begin() + static_cast<std::ptrdiff_t>(chunk));
      done += chunk;
    }
    (void)close_device(*dev);
    if (ok && total.size() == size) return total;
  }

  return Result<std::vector<std::uint8_t>>(
      {}, "No educational BYOVD device accepted PHYS_READ");
}

Result<void> write_physical_via_gdrv(std::uint64_t phys_addr,
                                     const std::vector<std::uint8_t>& data) {
  constexpr std::uint32_t kPhysWrite = 0xC3502004u;
  const char* candidates[] = {"\\\\.\\gdrv", "\\\\.\\GIO"};

  struct PhysReq {
    std::uint64_t phys_addr;
    std::uint32_t size;
    std::uint32_t reserved;
  };

  for (const char* path : candidates) {
    auto dev = open_device(path);
    if (!dev) continue;

    std::size_t done = 0;
    bool ok = true;
    while (done < data.size()) {
      const std::size_t chunk = (std::min)(data.size() - done, static_cast<std::size_t>(512));
      PhysReq req{phys_addr + done, static_cast<std::uint32_t>(chunk), 0};
      std::vector<std::uint8_t> in(sizeof(req) + chunk);
      std::memcpy(in.data(), &req, sizeof(req));
      std::memcpy(in.data() + sizeof(req), data.data() + done, chunk);
      auto out = send_ioctl(*dev, kPhysWrite, in, 0);
      if (!out) {
        ok = false;
        break;
      }
      done += chunk;
    }
    (void)close_device(*dev);
    if (ok) return Result<void>();
  }
  return Result<void>("No educational BYOVD device accepted PHYS_WRITE");
}

}  // namespace
#endif  // LR_PLATFORM_WINDOWS

// ── read_physical ──────────────────────────────────────────────────

Result<std::vector<std::uint8_t>> read_physical(std::uint64_t phys_addr, std::size_t size) {
#ifndef NDEBUG
  std::printf("[kernel:mem] read_physical: phys=0x%llx size=%zu\n",
              static_cast<unsigned long long>(phys_addr), size);
#endif
  if (size == 0) return std::vector<std::uint8_t>{};
  if (size > 16ull * 1024ull * 1024ull) {
    return Result<std::vector<std::uint8_t>>({}, "read_physical size exceeds 16 MiB cap");
  }

#if LR_PLATFORM_LINUX
  int fd = ::open("/dev/mem", O_RDONLY | O_SYNC);
  if (fd < 0) {
    fd = ::open("/dev/crash", O_RDONLY);
    if (fd < 0) return os_error("open /dev/mem or /dev/crash (need root)");
  }

  const std::size_t page_size = 4096;
  const std::uint64_t page_start = phys_addr & ~static_cast<std::uint64_t>(page_size - 1);
  const std::size_t offset = static_cast<std::size_t>(phys_addr - page_start);
  const std::size_t map_size = size + offset;

  void* map = ::mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd,
                     static_cast<off_t>(page_start));
  if (map == MAP_FAILED) {
    ::close(fd);
    return os_error("mmap /dev/mem");
  }

  std::vector<std::uint8_t> result(size);
  std::memcpy(result.data(), static_cast<std::uint8_t*>(map) + offset, size);
  ::munmap(map, map_size);
  ::close(fd);
  return result;

#elif LR_PLATFORM_WINDOWS
  // Prefer section mapping, then educational BYOVD PHYS_READ.
  auto via_section = read_physical_via_section(phys_addr, size);
  if (via_section) return via_section;

  auto via_gdrv = read_physical_via_gdrv(phys_addr, size);
  if (via_gdrv) return via_gdrv;

  // Honest combined failure: both real paths attempted.
  std::string msg = "Windows physical read denied: ";
  msg += via_section.error_msg.c_str();
  msg += "; ";
  msg += via_gdrv.error_msg.c_str();
  return Result<std::vector<std::uint8_t>>({}, msg);

#else
  (void)phys_addr;
  return Result<std::vector<std::uint8_t>>({}, "Physical memory read not supported");
#endif
}

// ── write_physical ─────────────────────────────────────────────────

Result<void> write_physical(std::uint64_t phys_addr,
                            const std::vector<std::uint8_t>& data) {
#ifndef NDEBUG
  std::printf("[kernel:mem] write_physical: phys=0x%llx size=%zu\n",
              static_cast<unsigned long long>(phys_addr), data.size());
#endif
  if (data.empty()) return Result<void>();

#if LR_PLATFORM_LINUX
  int fd = ::open("/dev/mem", O_RDWR | O_SYNC);
  if (fd < 0) return os_error("open /dev/mem writable (need root)");

  const std::size_t page_size = 4096;
  const std::uint64_t page_start = phys_addr & ~static_cast<std::uint64_t>(page_size - 1);
  const std::size_t offset = static_cast<std::size_t>(phys_addr - page_start);
  const std::size_t map_size = data.size() + offset;

  void* map = ::mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                     static_cast<off_t>(page_start));
  if (map == MAP_FAILED) {
    ::close(fd);
    return os_error("mmap /dev/mem for write");
  }

  std::memcpy(static_cast<std::uint8_t*>(map) + offset, data.data(), data.size());
  ::munmap(map, map_size);
  ::close(fd);
  return Result<void>();

#elif LR_PLATFORM_WINDOWS
  // Attempt writable PhysicalMemory section map first.
  auto& api = real::win::g_Api();
  using NtOpenSectionFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
  static NtOpenSectionFn pNtOpenSection = nullptr;
  static bool resolved = false;
  if (!resolved) {
    resolved = true;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
      pNtOpenSection = reinterpret_cast<NtOpenSectionFn>(
          GetProcAddress(ntdll, "NtOpenSection"));
    }
  }

  if (pNtOpenSection && api.NtMapViewOfSection && api.NtUnmapViewOfSection &&
      api.NtClose) {
    UNICODE_STRING obj_name{};
    wchar_t name_buf[] = L"\\Device\\PhysicalMemory";
    obj_name.Buffer = name_buf;
    obj_name.Length = static_cast<USHORT>(wcslen(name_buf) * sizeof(wchar_t));
    obj_name.MaximumLength = obj_name.Length + sizeof(wchar_t);
    OBJECT_ATTRIBUTES obj_attr{};
    InitializeObjectAttributes(&obj_attr, &obj_name, OBJ_CASE_INSENSITIVE,
                               nullptr, nullptr);
    HANDLE section = nullptr;
    NTSTATUS status =
        pNtOpenSection(&section, SECTION_MAP_READ | SECTION_MAP_WRITE, &obj_attr);
    if (NT_SUCCESS(status) && section) {
      void* view = nullptr;
      SIZE_T view_size = data.size();
      LARGE_INTEGER section_offset;
      section_offset.QuadPart = static_cast<LONGLONG>(phys_addr);
      constexpr ULONG kViewShare = 1;
      status = api.NtMapViewOfSection(
          section, reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)), &view, 0,
          data.size(), &section_offset, &view_size, kViewShare, 0, PAGE_READWRITE);
      if (NT_SUCCESS(status) && view != nullptr) {
        std::memcpy(view, data.data(), data.size());
        api.NtUnmapViewOfSection(
            reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)), view);
        api.NtClose(section);
        return Result<void>();
      }
      api.NtClose(section);
    }
  }

  auto via_gdrv = write_physical_via_gdrv(phys_addr, data);
  if (via_gdrv) return via_gdrv;

  return Result<void>(
      "Windows physical write denied: PhysicalMemory section and BYOVD PHYS_WRITE unavailable");

#else
  (void)phys_addr;
  (void)data;
  return Result<void>("Physical memory write not supported");
#endif
}

}  // namespace real::kernel::mem

