// port_io.cpp — Cross-platform x86 I/O port access for SMI/CMOS research.
//
// TECHNIQUE: outb/inb on 0xB2 (SMI), 0x70/0x71 (CMOS). On Windows x64,
// ring-3 IN/OUT #GP; lab uses WinRing0 / InpOut / EneIo class drivers.
//
// SCAR: Device open + IOCTL to known MSR/IO drivers is highly visible.
//
// BLUE: Block known IO-port driver hashes; alert on 0xB2 write patterns.
//
// MITIGATION: No usermode port I/O; SMM lockdown; remove vulnerable drivers.

#include "real/smm/smm_interface.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <sys/io.h>
#  include <unistd.h>
#elif LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#endif

namespace real::smm {
namespace {

#if LR_PLATFORM_WINDOWS
// WinRing0-style IOCTL codes (OpenLibSys).
constexpr DWORD kIoctlWriteIoPortByte = 0x9C402084;
constexpr DWORD kIoctlReadIoPortByte = 0x9C402088;

struct WinIoPortReq {
  ULONG port;
  UCHAR value;
  UCHAR reserved[3];
};

// Direct kernel32 imports — do not use g_Api()->CreateFileA (only filled
// after ensure_resolved(); null pointer would AV).
Result<HANDLE> open_io_driver() {
  const char* names[] = {
      "\\\\.\\WinRing0_1_2_0",
      "\\\\.\\WinRing0",
      "\\\\.\\InpOutx64",
      "\\\\.\\EneIo64",
      "\\\\.\\WinIo64",
      "\\\\.\\GLCKIo2",
  };
  for (const char* name : names) {
    HANDLE h = ::CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                             OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) return h;
  }
  return Result<HANDLE>(INVALID_HANDLE_VALUE, "No lab I/O port driver present");
}
#endif

}  // namespace

Result<void> port_outb(std::uint16_t port, std::uint8_t value) {
#if LR_PLATFORM_LINUX
  if (ioperm(port, 1, 1) == 0 || iopl(3) == 0) {
    outb(value, port);
    return Result<void>();
  }
  int fd = open("/dev/port", O_WRONLY);
  if (fd < 0) return os_error("open /dev/port (need root)");
  if (lseek(fd, static_cast<off_t>(port), SEEK_SET) < 0) {
    close(fd);
    return os_error("lseek /dev/port");
  }
  const ssize_t n = write(fd, &value, 1);
  close(fd);
  if (n != 1) return os_error("write /dev/port");
  return Result<void>();
#elif LR_PLATFORM_WINDOWS
  auto dev = open_io_driver();
  if (!dev) return Result<void>(dev.error_msg);
  WinIoPortReq req{};
  req.port = port;
  req.value = value;
  DWORD ret = 0;
  const BOOL ok = ::DeviceIoControl(*dev, kIoctlWriteIoPortByte, &req,
                                    sizeof(req), nullptr, 0, &ret, nullptr);
  ::CloseHandle(*dev);
  if (!ok) return os_error("DeviceIoControl WRITE_IO_PORT_BYTE");
  return Result<void>();
#else
  (void)port;
  (void)value;
  return Result<void>("I/O port access requires x86 Linux or Windows lab driver");
#endif
}

Result<std::uint8_t> port_inb(std::uint16_t port) {
#if LR_PLATFORM_LINUX
  if (ioperm(port, 1, 1) == 0 || iopl(3) == 0) {
    return static_cast<std::uint8_t>(inb(port));
  }
  int fd = open("/dev/port", O_RDONLY);
  if (fd < 0) {
    auto err = os_error("open /dev/port (need root)");
    return Result<std::uint8_t>(0, err.error_msg);
  }
  if (lseek(fd, static_cast<off_t>(port), SEEK_SET) < 0) {
    close(fd);
    auto err = os_error("lseek /dev/port");
    return Result<std::uint8_t>(0, err.error_msg);
  }
  std::uint8_t value = 0;
  const ssize_t n = read(fd, &value, 1);
  close(fd);
  if (n != 1) {
    auto err = os_error("read /dev/port");
    return Result<std::uint8_t>(0, err.error_msg);
  }
  return value;
#elif LR_PLATFORM_WINDOWS
  auto dev = open_io_driver();
  if (!dev) return Result<std::uint8_t>(0, dev.error_msg);
  WinIoPortReq req{};
  req.port = port;
  std::uint8_t out = 0;
  DWORD ret = 0;
  const BOOL ok = ::DeviceIoControl(*dev, kIoctlReadIoPortByte, &req,
                                    sizeof(req), &out, sizeof(out), &ret,
                                    nullptr);
  ::CloseHandle(*dev);
  if (!ok) {
    auto err = os_error("DeviceIoControl READ_IO_PORT_BYTE");
    return Result<std::uint8_t>(0, err.error_msg);
  }
  return out;
#else
  (void)port;
  return Result<std::uint8_t>(
      0, "I/O port access requires x86 Linux or Windows lab driver");
#endif
}

}  // namespace real::smm
