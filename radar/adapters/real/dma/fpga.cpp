// fpga.cpp -- FPGA/PCILeech-style DMA patterns for the educational DMA lab.

#include "real/dma/dma_backend.hpp"
#include "real/dma/page_walk.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include "real/win/api_table.hpp"
#  include <winioctl.h>
#elif LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace real::dma {

// BAR0 register map (aligned with firmware/example_pcie_dma educational layout).
namespace reg {
constexpr uint64_t kStatus = 0x4;
constexpr uint64_t kDoorbell = 0x8;
constexpr uint64_t kDescCount = 0x0;
constexpr uint64_t kDescRing = 0x1000;
constexpr uint64_t kDataBuffer = 0x2000;
}  // namespace reg

// Windows IOCTL codes for a PCILeech-compatible lab driver interface.
#if LR_PLATFORM_WINDOWS
constexpr DWORD kIoctlScatterRead =
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA00, METHOD_BUFFERED,
             FILE_READ_ACCESS | FILE_WRITE_ACCESS);
constexpr DWORD kIoctlWriteDesc =
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA01, METHOD_BUFFERED, FILE_WRITE_ACCESS);
constexpr DWORD kIoctlDoorbell =
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA02, METHOD_BUFFERED, FILE_WRITE_ACCESS);
constexpr DWORD kIoctlReadResult =
    CTL_CODE(FILE_DEVICE_UNKNOWN, 0xA03, METHOD_BUFFERED, FILE_READ_ACCESS);
#endif

Result<FpgaDmaDevice> fpga_dma_open(const std::string& device_path) {
#ifndef NDEBUG
  std::printf("[dma] fpga_dma_open: %s\n", device_path.c_str());
#endif
  if (device_path.empty()) {
    return Result<FpgaDmaDevice>({}, "FPGA device path is empty");
  }

#if LR_PLATFORM_LINUX
  const int fd = open(device_path.c_str(), O_RDWR | O_SYNC);
  if (fd < 0) return Result<FpgaDmaDevice>({}, "Cannot open FPGA device");
  FpgaDmaDevice device;
  device.device_path = device_path;
  device.fd = fd;
  device.use_device_io = false;
#ifndef NDEBUG
  std::printf("[dma] FPGA device opened: fd=%d\n", fd);
#endif
  return device;

#elif LR_PLATFORM_WINDOWS
  HANDLE handle =
      ::CreateFileA(device_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    return Result<FpgaDmaDevice>({}, "CreateFile FPGA failed");
  }
  FpgaDmaDevice device;
  device.device_path = device_path;
  device.fd = static_cast<int>(reinterpret_cast<intptr_t>(handle));
  device.use_device_io = true;
#ifndef NDEBUG
  std::printf("[dma] FPGA device opened (Windows handle)\n");
#endif
  return device;

#else
  (void)device_path;
  return Result<FpgaDmaDevice>({}, "FPGA DMA requires Linux or Windows");
#endif
}

Result<std::vector<uint8_t>> fpga_scatter_read(FpgaDmaDevice& device,
                                               uint64_t phys_addr, size_t size) {
#ifndef NDEBUG
  std::printf("[dma] fpga_scatter_read: phys=0x%llx size=%zu\n",
              static_cast<unsigned long long>(phys_addr), size);
#endif
  if (device.fd < 0) return Result<std::vector<uint8_t>>({}, "FPGA not open");
  if (size == 0) return std::vector<uint8_t>{};
  if (size > (16ull * 1024 * 1024)) {
    return Result<std::vector<uint8_t>>({}, "fpga_scatter_read: size exceeds 16 MiB");
  }

  // Pure descriptor construction — same unit tested independently.
  constexpr size_t kPageSize = 4096;
  auto descs = build_scatter_list(phys_addr, size, kPageSize);
  if (descs.empty()) {
    return Result<std::vector<uint8_t>>({}, "scatter list empty");
  }

#if LR_PLATFORM_LINUX
  const uint32_t descriptor_count = static_cast<uint32_t>(descs.size());
  if (pwrite(device.fd, &descriptor_count, sizeof(descriptor_count),
             reg::kDescCount) != static_cast<ssize_t>(sizeof(descriptor_count))) {
    return Result<std::vector<uint8_t>>({}, "FPGA descriptor-count write failed");
  }

  // Pack descriptors into a contiguous ring buffer (24 bytes each for this path).
  struct WireDesc {
    uint64_t src_phys;
    uint64_t dst_buf;
    uint32_t length;
    uint32_t flags;
  };
  std::vector<WireDesc> wire(descs.size());
  for (size_t i = 0; i < descs.size(); ++i) {
    wire[i] = {descs[i].src_phys, descs[i].dst_offset, descs[i].length,
               descs[i].flags};
  }
  const size_t wire_bytes = wire.size() * sizeof(WireDesc);
  if (pwrite(device.fd, wire.data(), wire_bytes, reg::kDescRing) < 0) {
    return Result<std::vector<uint8_t>>({}, "FPGA descriptor write failed");
  }

  const uint32_t doorbell = 1;
  if (pwrite(device.fd, &doorbell, sizeof(doorbell), reg::kDoorbell) !=
      static_cast<ssize_t>(sizeof(doorbell))) {
    return Result<std::vector<uint8_t>>({}, "FPGA doorbell write failed");
  }

  uint32_t status = 0xFFFFFFFF;
  for (int retry = 0; retry < 1000; ++retry) {
    if (pread(device.fd, &status, sizeof(status), reg::kStatus) ==
            static_cast<ssize_t>(sizeof(status)) &&
        status != 0xFFFFFFFF) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (status == 0xFFFFFFFF)
    return Result<std::vector<uint8_t>>({}, "FPGA DMA timeout");

  std::vector<uint8_t> result(size);
  const ssize_t received =
      pread(device.fd, result.data(), size, reg::kDataBuffer);
  if (received < 0)
    return Result<std::vector<uint8_t>>({}, "FPGA result read failed");
  result.resize(static_cast<size_t>(received));
#ifndef NDEBUG
  std::printf("[dma] FPGA scatter-gather DMA: %zu pages -> %zu bytes\n",
              descs.size(), result.size());
#endif
  return result;

#elif LR_PLATFORM_WINDOWS
  HANDLE handle = reinterpret_cast<HANDLE>(static_cast<intptr_t>(device.fd));

  // Request layout shared with lab drivers: phys + size → payload.
  struct ScatterReq {
    ULONGLONG phys;
    ULONG length;
    ULONG desc_count;
  } req{};
  req.phys = phys_addr;
  req.length = static_cast<ULONG>(size > 0xFFFFFFFFULL ? 0xFFFFFFFFUL : size);
  req.desc_count = static_cast<ULONG>(descs.size());

  std::vector<uint8_t> result(size);
  DWORD returned = 0;

  // Preferred path: single IOCTL that accepts the phys range and returns data.
  if (DeviceIoControl(handle, kIoctlScatterRead, &req, sizeof(req), result.data(),
                      static_cast<DWORD>(size), &returned, nullptr) &&
      returned > 0) {
    result.resize(returned);
#ifndef NDEBUG
    std::printf("[dma] FPGA Windows IOCTL scatter-read: %lu bytes (%zu descs)\n",
                returned, descs.size());
#endif
    return result;
  }

  // Multi-step path: write descriptors → doorbell → read result buffer.
  {
    struct WireDesc {
      ULONGLONG src_phys;
      ULONGLONG dst_offset;
      ULONG length;
      ULONG flags;
    };
    std::vector<WireDesc> wire(descs.size());
    for (size_t i = 0; i < descs.size(); ++i) {
      wire[i] = {descs[i].src_phys, descs[i].dst_offset, descs[i].length,
                 descs[i].flags};
    }
    DWORD w = 0;
    if (!DeviceIoControl(handle, kIoctlWriteDesc, wire.data(),
                         static_cast<DWORD>(wire.size() * sizeof(WireDesc)),
                         nullptr, 0, &w, nullptr)) {
      // Fall through to ReadFile-style path below.
    } else {
      ULONG doorbell = 1;
      DeviceIoControl(handle, kIoctlDoorbell, &doorbell, sizeof(doorbell),
                      nullptr, 0, &w, nullptr);

      for (int retry = 0; retry < 1000; ++retry) {
        returned = 0;
        if (DeviceIoControl(handle, kIoctlReadResult, &req, sizeof(req),
                            result.data(), static_cast<DWORD>(size), &returned,
                            nullptr) &&
            returned > 0) {
          result.resize(returned);
#ifndef NDEBUG
          std::printf(
              "[dma] FPGA Windows multi-IOCTL scatter: %lu bytes\n", returned);
#endif
          return result;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  // Final fallback: position + ReadFile (some char-device drivers expose this).
  LARGE_INTEGER li;
  li.QuadPart = static_cast<LONGLONG>(phys_addr);
  if (SetFilePointerEx(handle, li, nullptr, FILE_BEGIN)) {
    DWORD read = 0;
    if (::ReadFile(handle, result.data(), static_cast<DWORD>(size), &read,
                   nullptr) &&
        read > 0) {
      result.resize(read);
#ifndef NDEBUG
      std::printf("[dma] FPGA Windows ReadFile: %lu bytes\n", read);
#endif
      return result;
    }
  }

  return Result<std::vector<uint8_t>>(
      {}, "FPGA scatter-read failed on Windows device handle");

#else
  (void)descs;
  return Result<std::vector<uint8_t>>({}, "FPGA DMA requires Linux or Windows");
#endif
}

Result<void> fpga_dma_close(FpgaDmaDevice& device) {
  if (device.fd >= 0) {
#if LR_PLATFORM_LINUX
    close(device.fd);
#elif LR_PLATFORM_WINDOWS
    ::CloseHandle(reinterpret_cast<HANDLE>(static_cast<intptr_t>(device.fd)));
#endif
    device.fd = -1;
  }
  device.bar_base = 0;
  device.bar_size = 0;
  device.use_device_io = false;
  return Result<void>();
}

}  // namespace real::dma
