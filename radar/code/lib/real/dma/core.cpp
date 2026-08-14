// core.cpp -- Physical-memory access paths for the educational DMA lab.
//
// LESSON (read before using): Every DMA operation documented here leaves a
// detectable forensic scar. This code teaches anti-cheat developers what
// those scars are and how to mitigate them; it is not production cheat code.

#include "real/dma/dma_backend.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include "real/win/api_table.hpp"
#  include "real/win/xorstr.hpp"
#  include <winioctl.h>
#elif LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#endif

namespace real::dma {

// ── Physical Memory Read ───────────────────────────────────────────
//
// LESSON: Reading physical memory without a game process handle is the
// T4 approach. On Windows, \\.\PhysicalMemory is restricted after Win 7.
// On Linux, /dev/mem requires root. Both leave detectable artifacts:
//   - Opens to \Device\PhysicalMemory are logged by ETW
//   - /dev/mem reads show up in audit logs
//   - Kernel driver IOCTL reads leave device handle artifacts

Result<std::vector<uint8_t>> physmem_read(uint64_t phys_addr, size_t size) {
  if (size == 0) return std::vector<uint8_t>{};
  // Guard against absurd sizes that would OOM the lab process.
  if (size > (64ull * 1024 * 1024)) {
    return Result<std::vector<uint8_t>>({}, "physmem_read: size exceeds 64 MiB cap");
  }

  std::vector<uint8_t> buf(size);
#ifndef NDEBUG
  std::printf("[dma] physmem_read: phys_addr=0x%llx size=%zu\n",
              static_cast<unsigned long long>(phys_addr), size);
#endif

#if LR_PLATFORM_LINUX
  int fd = open("/dev/mem", O_RDONLY | O_SYNC);
  if (fd >= 0) {
    const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const uint64_t page_mask = static_cast<uint64_t>(page_size) - 1;
    const uint64_t page_aligned = phys_addr & ~page_mask;
    const size_t offset = static_cast<size_t>(phys_addr - page_aligned);
    const size_t map_size = size + offset;
    void* map = mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd,
                     static_cast<off_t>(page_aligned));
    if (map != MAP_FAILED) {
      std::memcpy(buf.data(), static_cast<uint8_t*>(map) + offset, size);
      munmap(map, map_size);
      close(fd);
      return buf;
    }
    // Fallback: pread may work on some kernels where mmap is blocked.
    const ssize_t n = pread(fd, buf.data(), size, static_cast<off_t>(phys_addr));
    close(fd);
    if (n > 0) {
      buf.resize(static_cast<size_t>(n));
      return buf;
    }
#ifndef NDEBUG
    std::printf("[dma] /dev/mem mmap/pread failed (need root or CAP_SYS_RAWIO)\n");
#endif
  }

  fd = open("/dev/crash", O_RDONLY);
  if (fd >= 0) {
    const ssize_t n = pread(fd, buf.data(), size, static_cast<off_t>(phys_addr));
    close(fd);
    if (n > 0) {
      buf.resize(static_cast<size_t>(n));
#ifndef NDEBUG
      std::printf("[dma] /dev/crash read: %zd bytes\n", n);
#endif
      return buf;
    }
  }

  // /proc/kcore contains a kernel virtual-memory image; this direct attempt
  // is educational and does not perform physical-to-virtual translation.
  fd = open("/proc/kcore", O_RDONLY);
  if (fd >= 0) {
    const ssize_t n = pread(fd, buf.data(), size, static_cast<off_t>(phys_addr));
    close(fd);
    if (n > 0) {
      buf.resize(static_cast<size_t>(n));
#ifndef NDEBUG
      std::printf("[dma] /proc/kcore read: %zd bytes (requires root)\n", n);
#endif
      return buf;
    }
  }

  return Result<std::vector<uint8_t>>(
      {}, "physmem_read failed: need root, CAP_SYS_RAWIO, or a kernel driver");

#elif LR_PLATFORM_WINDOWS
  // Use direct Win32/NT imports for robustness in tests and early init
  // (PEB+EAT table may still be unresolved). Educational scar remains the
  // PhysicalMemory open itself.
  auto& api = real::win::g_Api();
  api.ensure_resolved();

  // Path 1: \\.\PhysicalMemory (blocked on modern Windows for non-drivers).
  HANDLE h = ::CreateFileA(OBF("\\\\.\\PhysicalMemory"), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
  if (h != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER offset;
    offset.QuadPart = static_cast<LONGLONG>(phys_addr);
    DWORD read = 0;
    if (SetFilePointerEx(h, offset, nullptr, FILE_BEGIN) &&
        ::ReadFile(h, buf.data(), static_cast<DWORD>(size), &read, nullptr) &&
        read > 0) {
      buf.resize(read);
      ::CloseHandle(h);
#ifndef NDEBUG
      std::printf("[dma] \\\\.\\PhysicalMemory read: %lu bytes\n", read);
#endif
      return buf;
    }
    ::CloseHandle(h);
  }

  // Path 2: NtOpenSection(\\Device\\PhysicalMemory) + NtMapViewOfSection.
  using NtOpenSectionFn = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK,
                                           POBJECT_ATTRIBUTES);
  using NtMapViewFn = NTSTATUS(NTAPI*)(HANDLE, HANDLE, PVOID*, ULONG_PTR,
                                       SIZE_T, PLARGE_INTEGER, PSIZE_T, DWORD,
                                       ULONG, ULONG);
  using NtUnmapViewFn = NTSTATUS(NTAPI*)(HANDLE, PVOID);
  using NtCloseFn = NTSTATUS(NTAPI*)(HANDLE);

  static NtOpenSectionFn pNtOpenSection = nullptr;
  static NtMapViewFn pNtMapView = nullptr;
  static NtUnmapViewFn pNtUnmapView = nullptr;
  static NtCloseFn pNtClose = nullptr;
  static bool nt_resolved = false;
  if (!nt_resolved) {
    nt_resolved = true;
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
      pNtOpenSection = reinterpret_cast<NtOpenSectionFn>(
          ::GetProcAddress(ntdll, "NtOpenSection"));
      pNtMapView = reinterpret_cast<NtMapViewFn>(
          ::GetProcAddress(ntdll, "NtMapViewOfSection"));
      pNtUnmapView = reinterpret_cast<NtUnmapViewFn>(
          ::GetProcAddress(ntdll, "NtUnmapViewOfSection"));
      pNtClose =
          reinterpret_cast<NtCloseFn>(::GetProcAddress(ntdll, "NtClose"));
    }
  }
  // Prefer table pointers when resolved.
  if (api.NtMapViewOfSection)
    pNtMapView = reinterpret_cast<NtMapViewFn>(api.NtMapViewOfSection);
  if (api.NtUnmapViewOfSection)
    pNtUnmapView = reinterpret_cast<NtUnmapViewFn>(api.NtUnmapViewOfSection);
  if (api.NtClose) pNtClose = reinterpret_cast<NtCloseFn>(api.NtClose);

  if (pNtOpenSection && pNtMapView && pNtUnmapView && pNtClose) {
    UNICODE_STRING obj_name{};
    wchar_t name_buf[] = L"\\Device\\PhysicalMemory";
    obj_name.Buffer = name_buf;
    obj_name.Length = static_cast<USHORT>((wcslen(name_buf)) * sizeof(wchar_t));
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
      status = pNtMapView(section,
                          reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)),
                          &view, 0, size, &section_offset, &view_size,
                          kViewShare, 0, PAGE_READONLY);
      if (NT_SUCCESS(status) && view != nullptr) {
        std::memcpy(buf.data(), view, size);
        pNtUnmapView(reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)), view);
        pNtClose(section);
#ifndef NDEBUG
        std::printf(
            "[dma] NtMapViewOfSection(\\Device\\PhysicalMemory) succeeded\n");
#endif
        return buf;
      }
      pNtClose(section);
    }
  }

  // Path 3: attempt MmMapIoSpace-style access via a lab kernel driver.
  HANDLE drv = ::CreateFileA(OBF("\\\\.\\LegitRadarPhys"), GENERIC_READ, 0,
                             nullptr, OPEN_EXISTING, 0, nullptr);
  if (drv == INVALID_HANDLE_VALUE) {
    drv = ::CreateFileA(OBF("\\\\.\\WinRing0_1_2_0"), GENERIC_READ, 0, nullptr,
                        OPEN_EXISTING, 0, nullptr);
  }
  if (drv != INVALID_HANDLE_VALUE) {
    struct {
      ULONGLONG address;
      ULONG length;
      ULONG pad;
    } req{};
    req.address = phys_addr;
    req.length = static_cast<ULONG>(size > 0xFFFFFFFFULL ? 0xFFFFFFFFUL
                                                         : size);
    DWORD returned = 0;
    const DWORD kIoctls[] = {
        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_ACCESS),
        CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS),
        0x9C402428,
    };
    for (DWORD code : kIoctls) {
      if (DeviceIoControl(drv, code, &req, sizeof(req), buf.data(),
                          static_cast<DWORD>(size), &returned, nullptr) &&
          returned > 0) {
        buf.resize(returned);
        ::CloseHandle(drv);
#ifndef NDEBUG
        std::printf("[dma] kernel-driver phys read: %lu bytes\n", returned);
#endif
        return buf;
      }
    }
    ::CloseHandle(drv);
  }

  return Result<std::vector<uint8_t>>(
      {}, "physmem_read failed: needs kernel driver on modern Windows");

#elif defined(__APPLE__)
  (void)phys_addr;
  (void)size;
  return Result<std::vector<uint8_t>>(
      {}, "physmem_read: No physical memory interface on macOS (SIP blocks /dev/mem)");
#else
  (void)phys_addr;
  (void)size;
  return Result<std::vector<uint8_t>>({},
                                      "physmem_read not supported on this platform");
#endif
}

}  // namespace real::dma
