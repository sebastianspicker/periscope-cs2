// ioctl_interface.cpp — Complete IOCTL communication with kernel devices.
// Educational: every IOCTL call documents what blue detects.

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
#elif LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

namespace real::kernel {

bool is_valid_device_handle(DeviceHandle device) {
  if (device.native == 0) return false;
  // INVALID_HANDLE_VALUE on Windows is (HANDLE)-1
  if (device.native == static_cast<std::uint64_t>(static_cast<std::intptr_t>(-1))) {
    return false;
  }
  return true;
}

std::vector<std::string> device_path_candidates_from_name(const std::string& driver_or_device) {
  std::vector<std::string> out;
  if (driver_or_device.empty()) return out;

  std::string name = driver_or_device;
  // Already a full Win32 path?
  if (name.rfind("\\\\.\\", 0) == 0) {
    out.push_back(name);
    return out;
  }
  // Strip .sys
  auto dot = name.rfind(".sys");
  if (dot != std::string::npos && dot + 4 == name.size()) {
    name = name.substr(0, dot);
  }
  // Basename only
  auto slash = name.find_last_of("\\/");
  if (slash != std::string::npos) name = name.substr(slash + 1);

  out.push_back(std::string("\\\\.\\") + name);
  // Common educational aliases
  if (name != "gdrv") {
    // keep primary first
  }
  return out;
}

Result<DeviceHandle> open_device(const std::string& device_path) {
#ifndef NDEBUG
  std::printf("[kernel:ioctl] Opening device: %s\n", device_path.c_str());
#endif

#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  HANDLE h = INVALID_HANDLE_VALUE;
  if (api.CreateFileA) {
    h = api.CreateFileA(
        device_path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
  } else {
    h = CreateFileA(
        device_path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);
  }

  if (h == INVALID_HANDLE_VALUE) {
    return os_error("CreateFile on device");
  }

  DeviceHandle dev;
  dev.native = reinterpret_cast<std::uint64_t>(h);
#ifndef NDEBUG
  std::printf("[kernel:ioctl] Device opened: handle=0x%llx\n",
              static_cast<unsigned long long>(dev.native));
#endif
  return dev;

#elif LR_PLATFORM_LINUX
  // Linux character device open (e.g. /dev/mem, /dev/gdrv).
  int fd = ::open(device_path.c_str(), O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    fd = ::open(device_path.c_str(), O_RDONLY | O_CLOEXEC);
  }
  if (fd < 0) return os_error("open device");
  DeviceHandle dev;
  dev.native = static_cast<std::uint64_t>(fd);
  return dev;

#else
  (void)device_path;
  return Result<DeviceHandle>({}, "Device open is not supported on this platform");
#endif
}

Result<std::vector<std::uint8_t>> send_ioctl(
    DeviceHandle device,
    std::uint32_t ioctl_code,
    const std::vector<std::uint8_t>& input_data,
    std::size_t output_size) {
#ifndef NDEBUG
  std::printf("[kernel:ioctl] IOCTL: code=0x%08x in=%zu out=%zu\n",
              ioctl_code, input_data.size(), output_size);
#endif

  if (!is_valid_device_handle(device)) {
    return Result<std::vector<std::uint8_t>>({}, "Invalid device handle");
  }

#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  HANDLE h = reinterpret_cast<HANDLE>(device.native);

  std::vector<std::uint8_t> output(output_size ? output_size : 1);
  DWORD bytes_returned = 0;

  BOOL success = FALSE;
  if (api.DeviceIoControl) {
    success = api.DeviceIoControl(
        h,
        static_cast<DWORD>(ioctl_code),
        input_data.empty() ? nullptr : const_cast<std::uint8_t*>(input_data.data()),
        static_cast<DWORD>(input_data.size()),
        output_size ? output.data() : nullptr,
        static_cast<DWORD>(output_size),
        &bytes_returned,
        nullptr);
  } else {
    success = DeviceIoControl(
        h,
        static_cast<DWORD>(ioctl_code),
        input_data.empty() ? nullptr : const_cast<std::uint8_t*>(input_data.data()),
        static_cast<DWORD>(input_data.size()),
        output_size ? output.data() : nullptr,
        static_cast<DWORD>(output_size),
        &bytes_returned,
        nullptr);
  }

  if (!success) {
    return os_error("DeviceIoControl");
  }

  output.resize(bytes_returned);
#ifndef NDEBUG
  std::printf("[kernel:ioctl] IOCTL succeeded: %lu bytes returned\n", bytes_returned);
#endif
  return output;

#elif LR_PLATFORM_LINUX
  int fd = static_cast<int>(device.native);
  std::vector<std::uint8_t> buf;
  // Prefer a combined buffer large enough for in+out when output is requested.
  const std::size_t n = (std::max)(input_data.size(), output_size);
  buf.resize(n ? n : 1);
  if (!input_data.empty()) {
    std::memcpy(buf.data(), input_data.data(), input_data.size());
  }
  if (::ioctl(fd, static_cast<unsigned long>(ioctl_code), buf.data()) < 0) {
    return os_error("ioctl");
  }
  if (output_size) {
    buf.resize(output_size);
  } else {
    buf.clear();
  }
  return buf;

#else
  (void)ioctl_code;
  (void)input_data;
  (void)output_size;
  return Result<std::vector<std::uint8_t>>({}, "IOCTL is not supported on this platform");
#endif
}

Result<void> close_device(DeviceHandle device) {
  if (!is_valid_device_handle(device)) {
    return Result<void>();
  }

#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  HANDLE h = reinterpret_cast<HANDLE>(device.native);
  if (api.CloseHandle) {
    api.CloseHandle(h);
  } else {
    CloseHandle(h);
  }
#ifndef NDEBUG
  std::printf("[kernel:ioctl] Device handle closed\n");
#endif
  return Result<void>();

#elif LR_PLATFORM_LINUX
  ::close(static_cast<int>(device.native));
  return Result<void>();

#else
  (void)device;
  return Result<void>();
#endif
}

}  // namespace real::kernel
