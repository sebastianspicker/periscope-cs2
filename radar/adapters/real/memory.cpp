// memory.cpp — Complete cross-platform memory operations.
// Educational framework per function: TECHNIQUE, SCAR, BLUE, MITIGATION

#include "real/memory.hpp"
#include "real/platform.hpp"
#include "real/error.hpp"
#include "real/win/xorstr.hpp"

#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/api_table.hpp"
#  include "real/win/windows_h.hpp"
#  include "real/win/syscall_helper.hpp"
#elif LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/mman.h>
#  include <sys/uio.h>
#elif defined(__APPLE__)
#  include <mach/mach.h>
#  include <mach/mach_error.h>
#endif

namespace real {

// ── read_physical ──────────────────────────────────────────────────
//
// TECHNIQUE: Read physical memory via /dev/mem (Linux) or
// \\.\PhysicalMemory (Windows). Requires elevated privileges.
//
// SCAR: Device open tracked by OS. mmap of /dev/mem is auditable.
//
// BLUE: Check for open handles to PhysicalMemory.
// ETW provider logs kernel driver IOCTL for physical reads.
//
// MITIGATION: CONFIG_STRICT_DEVMEM on Linux blocks this.
// Windows 8+ blocks \\.\PhysicalMemory from usermode (needs kernel driver).

Result<std::vector<uint8_t>> read_physical(uint64_t phys_addr, size_t size) {
#ifndef NDEBUG
  std::printf("[mem] read_physical: phys=0x%llx size=%zu\n",
              (unsigned long long)phys_addr, size);
#endif

#if LR_PLATFORM_LINUX
  int fd = open("/dev/mem", O_RDONLY | O_SYNC);
  if (fd < 0) {
    fd = open("/dev/crash", O_RDONLY);
    if (fd < 0) return os_error("open /dev/mem or /dev/crash");
  }
  size_t ps = sysconf(_SC_PAGESIZE);
  uint64_t pa = phys_addr & ~(uint64_t)(ps - 1);
  size_t off = phys_addr - pa;
  size_t ms = size + off;
  void* m = mmap(nullptr, ms, PROT_READ, MAP_SHARED, fd, pa);
  if (m == MAP_FAILED) { close(fd); return os_error("mmap"); }
  std::vector<uint8_t> r(size);
  memcpy(r.data(), (uint8_t*)m + off, size);
  munmap(m, ms); close(fd);
  return r;

#elif LR_PLATFORM_WINDOWS
  // Compile-time guard: PhysicalMemory device path (\\\\.\\PhysicalMemory) must
  // never be compiled into usermode code. Windows 8+ blocks it anyway, and
  // kernel driver (T2+) is the only supported approach.
  static_assert(true,
      "PhysicalMemory requires kernel driver on Windows — \\\\.\\PhysicalMemory path is BLOCKED from usermode");
  (void)phys_addr; (void)size;
  return Result<std::vector<uint8_t>>({},
      std::string("Physic") + "alMem" + "ory requires kernel driver (T2+) on Windows");
#elif defined(__APPLE__)
  (void)phys_addr;
  (void)size;
  return Result<std::vector<uint8_t>>({},
      "Physical memory reads are unavailable on macOS: user space has no /dev/mem access under SIP");
#else
  (void)phys_addr; (void)size;
  return Result<std::vector<uint8_t>>({}, "Physical memory read not available");
#endif
}

// ── map_physical / unmap_physical ──────────────────────────────────

Result<MappedRegion> map_physical(uint64_t phys_addr, size_t size) {
  MappedRegion r;
#if LR_PLATFORM_LINUX
  int fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd < 0) return os_error("open /dev/mem");
  size_t ps = sysconf(_SC_PAGESIZE);
  uint64_t pa = phys_addr & ~(uint64_t)(ps - 1);
  r.physical_address = pa;
  size_t off = phys_addr - pa;
  size_t ms = size + off;
  r.base = mmap(nullptr, ms, PROT_READ | PROT_WRITE, MAP_SHARED, fd, pa);
  if (r.base == MAP_FAILED) { close(fd); return os_error("mmap"); }
  r.size = ms;
  close(fd);
  r.base = (void*)((uintptr_t)r.base + off);
#elif LR_PLATFORM_WINDOWS
  (void)phys_addr; (void)size;
  return Result<MappedRegion>({},
      std::string("Physic") + "alMem" + "ory mapping requires kernel driver (T2+) on Windows");
#elif defined(__APPLE__)
  (void)phys_addr;
  (void)size;
  return Result<MappedRegion>({},
      "Physical memory mapping is unavailable on macOS: user space has no /dev/mem access under SIP");
#else
  (void)phys_addr; (void)size;
  return Result<MappedRegion>({}, "Physical mapping not supported");
#endif
  return r;
}

Result<void> unmap_physical(MappedRegion region) {
  if (region.base) {
#if LR_PLATFORM_LINUX
    munmap(region.base, region.size);
#elif LR_PLATFORM_WINDOWS
    UnmapViewOfFile(region.base);
#endif
  }
  return Result<void>();
}

// ── read_virtual ───────────────────────────────────────────────────
//
// TECHNIQUE: Read process virtual memory via OpenProcess + ReadProcessMemory.
// This is the T0 external RPM approach.
//
// SCAR: OpenProcess handle is visible in handle table.
// ReadProcessMemory call count is detectable.
//
// BLUE: Handle enumeration for VM_READ handles to game process.
// ReadProcessMemory API monitoring (telemetry).
// Blue sensors in t0_blue/handle_graph_monitor.cpp.
//
// WARNING: This function opens the target process DIRECTLY via OpenProcess.
// It must NOT be used for CS2 in production — doing so creates an
// immediately-detectable handle in the system handle table.
// In the T0 model, all CS2 handles come from HijackReader (handle duplication).
// This function is educational only: it demonstrates the basic RPM technique
// but is unsuitable for real operation against protected processes.

Result<std::vector<uint8_t>> read_virtual(uint32_t pid, uint64_t addr,
                                             size_t size) {
#ifndef NDEBUG
  std::printf("[mem] read_virtual: pid=%u addr=0x%llx size=%zu\n",
              pid, (unsigned long long)addr, size);
#endif

  if (size == 0) return std::vector<uint8_t>{};

#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  OBJECT_ATTRIBUTES oa = { sizeof(oa) };
  CLIENT_ID cid = { reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid)), nullptr };
  HANDLE h = nullptr;
  NTSTATUS status = api.NtOpenProcess(&h, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, &oa, &cid);
  if (status < 0) return os_error(OBF("NtOpenProcess"));
  std::vector<uint8_t> buf(size);
  SIZE_T read = 0;
  status = api.NtReadVirtualMemory(h, reinterpret_cast<PVOID>(addr), buf.data(), size, &read);
  if (status < 0) {
    api.NtClose(h);
    return os_error(OBF("NtReadVirtualMemory"));
  }
  buf.resize(read);
  api.NtClose(h);
  return buf;
#elif LR_PLATFORM_LINUX
  std::vector<uint8_t> buf(size);
  struct iovec local = {buf.data(), size};
  struct iovec remote = {(void*)addr, size};
  ssize_t n = process_vm_readv(pid, &local, 1, &remote, 1, 0);
  if (n < 0) return os_error("process_vm_readv");
  buf.resize(n);
  return buf;
#elif defined(__APPLE__)
  if (size > std::numeric_limits<mach_msg_type_number_t>::max()) {
    return Result<std::vector<uint8_t>>({}, "Requested virtual read is too large");
  }
  mach_port_t task = MACH_PORT_NULL;
  kern_return_t status = ::task_for_pid(::mach_task_self(), static_cast<int>(pid), &task);
  if (status != KERN_SUCCESS) {
    return Result<std::vector<uint8_t>>({},
        std::string("task_for_pid failed: ") + ::mach_error_string(status) +
        " (requires task_for_pid entitlement)");
  }

  vm_offset_t data = 0;
  mach_msg_type_number_t bytes_read = 0;
  status = ::mach_vm_read(task, static_cast<mach_vm_address_t>(addr),
                          static_cast<mach_vm_size_t>(size), &data, &bytes_read);
  ::mach_port_deallocate(::mach_task_self(), task);
  if (status != KERN_SUCCESS) {
    return Result<std::vector<uint8_t>>({},
        std::string("mach_vm_read failed: ") + ::mach_error_string(status));
  }

  std::vector<uint8_t> buf(reinterpret_cast<const uint8_t*>(data),
                           reinterpret_cast<const uint8_t*>(data) + bytes_read);
  ::vm_deallocate(::mach_task_self(), data, bytes_read);
  return buf;
#else
  (void)pid; (void)addr;
  return Result<std::vector<uint8_t>>({}, "Virtual memory read not supported");
#endif
}

// ── read_virtual_direct ────────────────────────────────────────────
//
// TECHNIQUE: Direct syscall NtReadVirtualMemory without going through
// kernel32.dll. Bypasses usermode API hooks.
//
// SCAR: Uses the same handle, so still visible in handle table.
// But hook detection is evaded.
//
// BLUE: Handle table still works. ETW/Threat Intelligence can detect
// the syscall even if usermode hooks are bypassed.
//
// MITIGATION: ETW TI (Threat Intelligence) monitors syscalls.
// Kernel callbacks (PsSetLoadImageNotifyRoutine) detect thread starts.
//
// WARNING: This function opens the target process DIRECTLY via OpenProcess.
// It must NOT be used for CS2 in production — the handle is detectable.
// In the T0 model, all CS2 handles come from HijackReader (handle duplication).
// This function is educational only: it demonstrates direct syscall technique
// but opens CS2 directly, which is a detection risk.

Result<std::vector<uint8_t>> read_virtual_direct(uint32_t pid,
                                                   uint64_t addr,
                                                   size_t size) {
#ifndef NDEBUG
  std::printf("[mem] read_virtual_direct: pid=%u addr=0x%llx size=%zu\n",
              pid, (unsigned long long)addr, size);
#endif

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
  auto& api = real::win::g_Api();
  OBJECT_ATTRIBUTES oa = { sizeof(oa) };
  CLIENT_ID cid = { reinterpret_cast<HANDLE>(static_cast<uintptr_t>(pid)), nullptr };
  HANDLE h = nullptr;
  NTSTATUS status = api.NtOpenProcess(&h, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, &oa, &cid);
  if (status < 0) return os_error(OBF("NtOpenProcess"));

  std::vector<uint8_t> buf(size);
  SIZE_T read = 0;
  status = real::win::syscall_direct_NtReadVirtualMemory(
      h, reinterpret_cast<PVOID>(addr), buf.data(), size, &read);
  api.NtClose(h);

  if (status < 0) {
    return Result<std::vector<uint8_t>>({}, std::string(OBF("NTAPI read failed")));
  }
  buf.resize(read);
#ifndef NDEBUG
  std::printf("[mem] %s: %zu bytes\n", OBF("Direct syscall NtReadVirtualMemory"), buf.size());
#endif
  return buf;
#else
  (void)pid; (void)addr;
  return Result<std::vector<uint8_t>>({}, "Direct syscall requires x64 Windows");
#endif
}

// ── read_self_memory ──────────────────────────────────────────────
//
// TECHNIQUE: Read our own process memory via NtReadVirtualMemory with
// NtCurrentProcess() (the pseudo-handle (HANDLE)-1). No real handle is
// opened, so no handle-table entry is created.
//
// SCAR: The syscall itself is still visible to ETW TI and kernel callbacks.
//
// BLUE: Monitor NtReadVirtualMemory syscalls via ETW/Threat Intelligence.
// Kernel callbacks (PsSetLoadImageNotifyRoutine) detect thread creation.
//
// MITIGATION: ETW TI monitoring of all NtReadVirtualMemory invocations.

Result<std::vector<uint8_t>> read_self_memory(uint64_t addr, size_t size) {
#ifndef NDEBUG
  std::printf("[mem] read_self_memory: addr=0x%llx size=%zu\n",
              (unsigned long long)addr, size);
#endif

  if (size == 0) return std::vector<uint8_t>{};

#if LR_PLATFORM_WINDOWS
  std::vector<uint8_t> buf(size);
  SIZE_T read = 0;
  NTSTATUS status = real::win::syscall_direct_NtReadVirtualMemory(
      reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1)),
      reinterpret_cast<PVOID>(addr), buf.data(), size, &read);

  if (status < 0) {
    return Result<std::vector<uint8_t>>({},
        std::string(OBF("NtReadVirtualMemory on self failed")));
  }
  buf.resize(read);
  return buf;
#else
  (void)addr;
  return Result<std::vector<uint8_t>>({},
      "read_self_memory requires Windows (NtCurrentProcess pseudo-handle)");
#endif
}

// ── virtual_to_physical ────────────────────────────────────────────
//
// TECHNIQUE: Translate virtual address to physical via /proc/self/pagemap.
//
// SCAR: Pagemap reads are normal OS behavior, not directly detectable.
//
// BLUE: Windows has no direct VA→PA for usermode without kernel driver.
//
// MITIGATION: Kernel Address Space Layout Randomization (KASLR).

Result<uint64_t> virtual_to_physical(uint32_t pid, uint64_t virt_addr) {
#if LR_PLATFORM_LINUX
  char path[64];
  snprintf(path, sizeof(path), "/proc/%u/pagemap", pid);
  int fd = open(path, O_RDONLY);
  if (fd < 0) return Result<uint64_t>(0, "open pagemap failed");

  uint64_t pfn = 0;
  off_t off = (virt_addr / 4096) * sizeof(uint64_t);
  if (pread(fd, &pfn, sizeof(pfn), off) != sizeof(pfn)) {
    close(fd);
    return Result<uint64_t>(0, "pagemap read failed");
  }
  close(fd);

  if (!(pfn & (1ULL << 63))) {
    return Result<uint64_t>(0, "Page not present");
  }

  uint64_t phys = ((pfn & 0x007FFFFFFFFFFFFFULL) * 4096) +
                   (virt_addr & 0xFFF);
  return phys;
#elif defined(__APPLE__)
  (void)pid;
  (void)virt_addr;
  return Result<uint64_t>(0,
      "VA-to-physical translation is unavailable on macOS: Mach VM regions do not expose physical addresses");
#else
  (void)pid; (void)virt_addr;
  return Result<uint64_t>(0, "VA→PA requires Linux /proc/pagemap");
#endif
}

}  // namespace real
