// ioctl_interface.hpp — Device open / IOCTL / close for kernel device objects.
//
// LESSON: Device opens and IOCTL requests are observable through handle tables,
// syscall telemetry, and IRP tracing. This backend performs real Windows
// CreateFile / DeviceIoControl / CloseHandle (via ApiTable when available).

#pragma once

#include "real/error.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace real::kernel {

struct DeviceHandle {
  std::uint64_t native = 0;
  explicit operator bool() const { return native != 0; }
};

inline bool is_valid_device_handle(DeviceHandle device) {
  if (device.native == 0) return false;
  // INVALID_HANDLE_VALUE on Windows is (HANDLE)-1.
  return device.native != static_cast<std::uint64_t>(static_cast<std::intptr_t>(-1));
}


/// Open a kernel device object (e.g. "\\\\.\\gdrv").
Result<DeviceHandle> open_device(const std::string& device_path);

/// Submit an IOCTL; returns the output buffer bytes actually written.
Result<std::vector<std::uint8_t>> send_ioctl(DeviceHandle device,
                                             std::uint32_t ioctl_code,
                                             const std::vector<std::uint8_t>& input_data,
                                             std::size_t output_size);

/// Close a previously opened device handle (idempotent).
Result<void> close_device(DeviceHandle device);

// ── E17 hardening: RAII ownership + overflow-checked chunk planning ──

/// RAII owner for a device handle: the handle is closed exactly once, on
/// every exit path (the manual close-per-branch pattern this replaces is
/// where handle leaks come from — assessment ledger E17). Movable, not
/// copyable.
class ScopedDevice {
 public:
  ScopedDevice() = default;
  explicit ScopedDevice(DeviceHandle device) : device_(device) {}
  ~ScopedDevice() {
    if (is_valid_device_handle(device_)) {
      (void)close_device(device_);
    }
  }
  ScopedDevice(const ScopedDevice&) = delete;
  ScopedDevice& operator=(const ScopedDevice&) = delete;
  ScopedDevice(ScopedDevice&& other) noexcept : device_(other.device_) {
    other.device_.native = 0;
  }
  ScopedDevice& operator=(ScopedDevice&& other) noexcept {
    if (this != &other) {
      if (is_valid_device_handle(device_)) (void)close_device(device_);
      device_ = other.device_;
      other.device_.native = 0;
    }
    return *this;
  }

  DeviceHandle get() const { return device_; }
  bool valid() const { return is_valid_device_handle(device_); }

  /// Open and take ownership; an unopenable device yields an invalid guard.
  static ScopedDevice open(const std::string& device_path) {
    auto opened = open_device(device_path);
    if (!opened) return ScopedDevice();
    return ScopedDevice(*opened);
  }

 private:
  DeviceHandle device_{};
};

/// Overflow-checked address arithmetic for chunk loops: returns false when
/// ``address + offset + size`` would wrap.
inline bool chunk_range_ok(std::uint64_t address, std::size_t offset,
                           std::size_t size) {
  const std::uint64_t start = address + offset;  // wraps defined for unsigned
  if (start < address && offset != 0) return false;
  const std::uint64_t end = start + size;
  if (end < start) return false;
  return true;
}

/// One planned chunk of a large read.
struct ChunkPlan {
  std::uint64_t address = 0;
  std::size_t size = 0;
};

/// Split ``[address, address+total)`` into device-sized chunks (ledger E17:
/// chunk boundaries are explicit, overflow-checked, and testable — the
/// callers never hand-write the loop math).
inline std::vector<ChunkPlan> plan_chunked_read(std::uint64_t address,
                                                std::size_t total,
                                                std::size_t max_chunk) {
  std::vector<ChunkPlan> plan;
  if (total == 0 || max_chunk == 0) return plan;
  std::size_t done = 0;
  while (done < total) {
    const std::size_t chunk = total - done < max_chunk ? total - done : max_chunk;
    if (!chunk_range_ok(address, done, chunk)) {
      plan.clear();  // range would overflow: refuse the whole read
      return plan;
    }
    plan.push_back(ChunkPlan{address + done, chunk});
    done += chunk;
  }
  return plan;
}

/// Page-aware variant: never lets a chunk straddle a 4 KiB page boundary
/// (E69). Useful for hypercall/IOCTL paths where a mapping boundary mid-chunk
/// would fault or split the target page.
inline std::vector<ChunkPlan> plan_chunked_read_pages(std::uint64_t address,
                                                      std::size_t total,
                                                      std::size_t max_chunk,
                                                      std::size_t page_size = 0x1000) {
  std::vector<ChunkPlan> plan;
  if (total == 0 || max_chunk == 0 || page_size == 0) return plan;
  std::size_t done = 0;
  while (done < total) {
    const std::uint64_t current = address + done;
    const std::size_t page_room =
        static_cast<std::size_t>(page_size - (current & (page_size - 1)));
    std::size_t chunk = total - done < max_chunk ? total - done : max_chunk;
    if (chunk > page_room) chunk = page_room;
    if (!chunk_range_ok(address, done, chunk)) {
      plan.clear();
      return plan;
    }
    plan.push_back(ChunkPlan{address + done, chunk});
    done += chunk;
  }
  return plan;
}

/// CTL_CODE macro as a constexpr pure helper (matches Windows SDK layout).
constexpr std::uint32_t ctl_code(int device_type, int function, int method, int access) {
  return (static_cast<std::uint32_t>(device_type) << 16) |
         (static_cast<std::uint32_t>(access) << 14) |
         (static_cast<std::uint32_t>(function) << 2) |
         static_cast<std::uint32_t>(method);
}

enum IoctlMethod {
  METHOD_BUFFERED = 0,
  METHOD_IN_DIRECT = 1,
  METHOD_OUT_DIRECT = 2,
  METHOD_NEITHER = 3
};

enum FileDeviceType {
  FILE_DEVICE_UNKNOWN = 0x22,
  FILE_DEVICE_ACPI = 0x32,
  FILE_DEVICE_PHYSICAL_MEMORY = 0x44
};

/// Common lab / suspicious IOCTL codes used by educational backends.
enum SuspiciousIoctlCodes {
  IOCTL_MEM_READ =
      ctl_code(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, 0),
  IOCTL_MEM_WRITE =
      ctl_code(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, 0),
  IOCTL_PHYSMEM_READ =
      ctl_code(FILE_DEVICE_PHYSICAL_MEMORY, 0x800, METHOD_OUT_DIRECT, 0),
  IOCTL_PROTECT_DISABLE =
      ctl_code(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, 3),
  IOCTL_GET_BASE =
      ctl_code(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, 0),
};

/// True if the handle looks like a live native handle (non-zero, not -1).

/// Build candidate Win32 device paths from a driver basename (no I/O).
std::vector<std::string> device_path_candidates_from_name(const std::string& driver_or_device);

}  // namespace real::kernel
