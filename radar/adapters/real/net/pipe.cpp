// pipe.cpp — Cross-platform named-pipe IPC with framed messages.

#include "real/net/net_client.hpp"
#include "real/net/net_internal.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#else
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <errno.h>
#  include <poll.h>
#endif

namespace real::net {

NamedPipe::~NamedPipe() { close(); }

Result<void> NamedPipe::create_server(const std::string& pipe_name) {
  auto r = create_server_nowait(pipe_name);
  if (!r) return r;
  return wait_for_client(-1);
}

Result<void> NamedPipe::create_server_nowait(const std::string& pipe_name) {
  if (is_open()) close();
#if LR_PLATFORM_WINDOWS
  HANDLE h = CreateNamedPipeA(
      pipe_name.c_str(),
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, nullptr);
  // Fall back without OVERLAPPED if needed
  if (h == INVALID_HANDLE_VALUE) {
    h = CreateNamedPipeA(pipe_name.c_str(), PIPE_ACCESS_DUPLEX,
                         PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                         PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, nullptr);
  }
  if (h == INVALID_HANDLE_VALUE) return os_error("CreateNamedPipe");
  handle_ = reinterpret_cast<std::intptr_t>(h);
  is_server_ = true;
#else
  unlink(pipe_name.c_str());
  if (mkfifo(pipe_name.c_str(), 0666) < 0) return os_error("mkfifo");
  int fd = ::open(pipe_name.c_str(), O_RDWR | O_NONBLOCK);
  if (fd < 0) return os_error("open fifo");
  // Switch to blocking after open
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags >= 0) fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
  handle_ = static_cast<std::intptr_t>(fd);
  is_server_ = true;
#endif
  return Result<void>();
}

Result<void> NamedPipe::wait_for_client(int timeout_ms) {
  if (!is_open() || !is_server_)
    return Result<void>("Not a server pipe");
#if LR_PLATFORM_WINDOWS
  HANDLE h = reinterpret_cast<HANDLE>(handle_);
  OVERLAPPED ov{};
  ov.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  BOOL ok = ConnectNamedPipe(h, ov.hEvent ? &ov : nullptr);
  if (!ok) {
    const DWORD err = GetLastError();
    if (err == ERROR_PIPE_CONNECTED) {
      if (ov.hEvent) CloseHandle(ov.hEvent);
      return Result<void>();
    }
    if (err == ERROR_IO_PENDING && ov.hEvent) {
      const DWORD wr = WaitForSingleObject(
          ov.hEvent, timeout_ms < 0 ? INFINITE : static_cast<DWORD>(timeout_ms));
      if (wr != WAIT_OBJECT_0) {
        CancelIo(h);
        CloseHandle(ov.hEvent);
        return Result<void>("wait_for_client timed out");
      }
      DWORD xferred = 0;
      if (!GetOverlappedResult(h, &ov, &xferred, FALSE)) {
        CloseHandle(ov.hEvent);
        return os_error("GetOverlappedResult ConnectNamedPipe");
      }
      CloseHandle(ov.hEvent);
      return Result<void>();
    }
    if (ov.hEvent) CloseHandle(ov.hEvent);
    // Blocking path without overlapped
    if (!ConnectNamedPipe(h, nullptr)) {
      if (GetLastError() != ERROR_PIPE_CONNECTED)
        return os_error("ConnectNamedPipe");
    }
  } else if (ov.hEvent) {
    CloseHandle(ov.hEvent);
  }
  return Result<void>();
#else
  (void)timeout_ms;
  // FIFO: writer presence is implicit on first read/write
  return Result<void>();
#endif
}

Result<void> NamedPipe::connect_client(const std::string& pipe_name) {
  return connect_client(pipe_name, 30000);
}

Result<void> NamedPipe::connect_client(const std::string& pipe_name, int timeout_ms) {
  if (is_open()) close();
#if LR_PLATFORM_WINDOWS
  const DWORD deadline = GetTickCount() + static_cast<DWORD>(timeout_ms < 0 ? 60000 : timeout_ms);
  for (;;) {
    HANDLE h = CreateFileA(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE,
                           0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
      handle_ = reinterpret_cast<std::intptr_t>(h);
      is_server_ = false;
      DWORD mode = PIPE_READMODE_BYTE;
      SetNamedPipeHandleState(h, &mode, nullptr, nullptr);
      return Result<void>();
    }
    if (GetLastError() != ERROR_PIPE_BUSY)
      return os_error("CreateFile pipe");
    const DWORD wait = (timeout_ms < 0)
        ? NMPWAIT_WAIT_FOREVER
        : (deadline > GetTickCount() ? (deadline - GetTickCount()) : 0);
    if (wait == 0) return Result<void>("connect_client timed out");
    if (!WaitNamedPipeA(pipe_name.c_str(), wait))
      return os_error("WaitNamedPipe");
  }
#else
  (void)timeout_ms;
  int fd = ::open(pipe_name.c_str(), O_RDWR);
  if (fd < 0) return os_error("open pipe");
  handle_ = static_cast<std::intptr_t>(fd);
  is_server_ = false;
  return Result<void>();
#endif
}

Result<int> NamedPipe::write(const std::uint8_t* data, int size) {
  if (!is_open()) return Result<int>(-1, "Pipe not open");
  if (size < 0) return Result<int>(-1, "negative size");
  if (size == 0) return 0;
#if LR_PLATFORM_WINDOWS
  HANDLE h = reinterpret_cast<HANDLE>(handle_);
  DWORD written = 0;
  if (!WriteFile(h, data, static_cast<DWORD>(size), &written, nullptr))
    return os_error("WriteFile pipe");
  return static_cast<int>(written);
#else
  const int written = static_cast<int>(::write(static_cast<int>(handle_), data,
                                               static_cast<std::size_t>(size)));
  if (written < 0) return os_error("write pipe");
  return written;
#endif
}

Result<int> NamedPipe::read(std::uint8_t* buffer, int buffer_size) {
  if (!is_open()) return Result<int>(-1, "Pipe not open");
  if (buffer_size <= 0) return Result<int>(-1, "invalid buffer");
#if LR_PLATFORM_WINDOWS
  HANDLE h = reinterpret_cast<HANDLE>(handle_);
  DWORD nread = 0;
  if (!ReadFile(h, buffer, static_cast<DWORD>(buffer_size), &nread, nullptr))
    return os_error("ReadFile pipe");
  return static_cast<int>(nread);
#else
  const int r = static_cast<int>(::read(static_cast<int>(handle_), buffer,
                                        static_cast<std::size_t>(buffer_size)));
  if (r < 0) return os_error("read pipe");
  return r;
#endif
}

Result<void> NamedPipe::write_frame(const std::uint8_t* data, int size) {
  if (size < 0) return Result<void>("negative frame size");
  std::uint8_t hdr[4];
  write_u32_le(hdr, static_cast<std::uint32_t>(size));
  auto w = write(hdr, 4);
  if (!w) return Result<void>(w.error_msg);
  if (*w != 4) return Result<void>("short write on frame header");
  int off = 0;
  while (off < size) {
    auto n = write(data + off, size - off);
    if (!n) return Result<void>(n.error_msg);
    if (*n == 0) return Result<void>("pipe write returned 0");
    off += *n;
  }
  return Result<void>();
}

Result<std::vector<std::uint8_t>> NamedPipe::read_frame(int max_size) {
  std::uint8_t hdr[4];
  int got = 0;
  while (got < 4) {
    auto n = read(hdr + got, 4 - got);
    if (!n) return Result<std::vector<std::uint8_t>>({}, n.error_msg);
    if (*n == 0) return Result<std::vector<std::uint8_t>>({}, "pipe closed on frame header");
    got += *n;
  }
  const std::uint32_t len = read_u32_le(hdr);
  if (static_cast<int>(len) > max_size)
    return Result<std::vector<std::uint8_t>>({}, "frame exceeds max_size");
  std::vector<std::uint8_t> body(len);
  std::uint32_t off = 0;
  while (off < len) {
    auto n = read(body.data() + off, static_cast<int>(len - off));
    if (!n) return Result<std::vector<std::uint8_t>>({}, n.error_msg);
    if (*n == 0) return Result<std::vector<std::uint8_t>>({}, "pipe closed on frame body");
    off += static_cast<std::uint32_t>(*n);
  }
  return body;
}

void NamedPipe::close() {
  if (handle_ != -1) {
#if LR_PLATFORM_WINDOWS
    HANDLE h = reinterpret_cast<HANDLE>(handle_);
    if (is_server_) FlushFileBuffers(h);
    DisconnectNamedPipe(h);
    CloseHandle(h);
#else
    ::close(static_cast<int>(handle_));
#endif
    handle_ = -1;
  }
  is_server_ = false;
}

}  // namespace real::net
