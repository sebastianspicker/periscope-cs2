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

/// Open a kernel device object (e.g. "\\\\.\\gdrv").
Result<DeviceHandle> open_device(const std::string& device_path);

/// Submit an IOCTL; returns the output buffer bytes actually written.
Result<std::vector<std::uint8_t>> send_ioctl(DeviceHandle device,
                                             std::uint32_t ioctl_code,
                                             const std::vector<std::uint8_t>& input_data,
                                             std::size_t output_size);

/// Close a previously opened device handle (idempotent).
Result<void> close_device(DeviceHandle device);

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
bool is_valid_device_handle(DeviceHandle device);

/// Build candidate Win32 device paths from a driver basename (no I/O).
std::vector<std::string> device_path_candidates_from_name(const std::string& driver_or_device);

}  // namespace real::kernel
