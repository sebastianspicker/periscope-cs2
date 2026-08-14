// socket.cpp — Cross-platform sockets: connect, listen, accept, UDP,
// timeouts, nodelay, keepalive, send_all / recv_exact, proxy-aware connect_ex.

#include "real/net/net_client.hpp"
#include "real/net/net_internal.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
using socklen_t_compat = int;
#else
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <poll.h>
#  include <errno.h>
using socklen_t_compat = socklen_t;
#endif

namespace real::net {

namespace {

Result<void> set_blocking(int fd, bool blocking) {
#if LR_PLATFORM_WINDOWS
  u_long mode = blocking ? 0 : 1;
  if (ioctlsocket(fd, FIONBIO, &mode) != 0)
    return os_error("ioctlsocket FIONBIO");
#else
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0) return os_error("fcntl F_GETFL");
  if (blocking) flags &= ~O_NONBLOCK;
  else flags |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flags) < 0) return os_error("fcntl F_SETFL");
#endif
  return Result<void>();
}

Result<void> wait_connected(int fd, int timeout_ms) {
#if LR_PLATFORM_WINDOWS
  fd_set wset, eset;
  FD_ZERO(&wset);
  FD_ZERO(&eset);
  FD_SET(static_cast<SOCKET>(fd), &wset);
  FD_SET(static_cast<SOCKET>(fd), &eset);
  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  const int rc = select(0, nullptr, &wset, &eset, timeout_ms >= 0 ? &tv : nullptr);
  if (rc == 0) return Result<void>("connect timed out");
  if (rc < 0) return os_error("select connect");
  int so_error = 0;
  int len = sizeof(so_error);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&so_error), &len) != 0)
    return os_error("getsockopt SO_ERROR");
  if (so_error != 0)
    return Result<void>("connect failed: " + format_os_error(static_cast<unsigned long>(so_error)));
  return Result<void>();
#else
  pollfd pfd{};
  pfd.fd = fd;
  pfd.events = POLLOUT;
  const int rc = poll(&pfd, 1, timeout_ms);
  if (rc == 0) return Result<void>("connect timed out");
  if (rc < 0) return os_error("poll connect");
  int so_error = 0;
  socklen_t len = sizeof(so_error);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &len) != 0)
    return os_error("getsockopt SO_ERROR");
  if (so_error != 0)
    return Result<void>("connect failed: " + format_os_error(static_cast<unsigned long>(so_error)));
  return Result<void>();
#endif
}

Result<Socket> connect_direct(const std::string& host, std::uint16_t port,
                               Protocol proto, int timeout_ms,
                               bool nodelay, bool keepalive) {
#if LR_PLATFORM_WINDOWS
  auto ws = ensure_winsock();
  if (!ws) return Result<Socket>(Socket{-1}, ws.error_msg);
#endif
  const int type = (proto == Protocol::Tcp) ? SOCK_STREAM : SOCK_DGRAM;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = type;
  hints.ai_protocol = (proto == Protocol::Tcp) ? IPPROTO_TCP : IPPROTO_UDP;

  const std::string port_str = std::to_string(port);
  addrinfo* result = nullptr;
  const int rc = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result);
  if (rc != 0 || !result) {
    return Result<Socket>(Socket{-1},
        "DNS resolution failed for " + host + ": " + gai_strerror(rc));
  }

  int fd = -1;
  std::string last_err = "connect failed";
  for (addrinfo* ai = result; ai != nullptr; ai = ai->ai_next) {
    fd = static_cast<int>(::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol));
    if (fd < 0) {
      last_err = "socket create failed";
      continue;
    }

    if (proto == Protocol::Tcp && timeout_ms > 0) {
      auto nb = set_blocking(fd, false);
      if (!nb) {
        close_socket(fd);
        fd = -1;
        last_err = nb.error_msg.c_str();
        continue;
      }
      const int cr = ::connect(fd, ai->ai_addr, static_cast<socklen_t_compat>(ai->ai_addrlen));
#if LR_PLATFORM_WINDOWS
      const bool in_progress = (cr < 0 && WSAGetLastError() == WSAEWOULDBLOCK);
#else
      const bool in_progress = (cr < 0 && (errno == EINPROGRESS || errno == EWOULDBLOCK));
#endif
      if (cr < 0 && !in_progress) {
        last_err = "connect failed";
        close_socket(fd);
        fd = -1;
        continue;
      }
      if (cr < 0) {
        auto w = wait_connected(fd, timeout_ms);
        if (!w) {
          last_err = w.error_msg.c_str();
          close_socket(fd);
          fd = -1;
          continue;
        }
      }
      set_blocking(fd, true);
    } else {
      if (::connect(fd, ai->ai_addr, static_cast<socklen_t_compat>(ai->ai_addrlen)) < 0) {
        last_err = "connect failed";
        close_socket(fd);
        fd = -1;
        continue;
      }
    }
    break;
  }
  freeaddrinfo(result);

  if (fd < 0)
    return Result<Socket>(Socket{-1}, last_err);

  Socket sock{fd};
  if (proto == Protocol::Tcp) {
    if (nodelay) set_nodelay(sock, true);
    if (keepalive) set_keepalive(sock, true);
  }
  return sock;
}

}  // namespace

Result<Socket> connect(const std::string& host, std::uint16_t port, Protocol proto) {
  ConnectOptions opts;
  return connect_ex(host, port, opts, proto);
}

Result<Socket> connect_ex(const std::string& host, std::uint16_t port,
                           const ConnectOptions& opts, Protocol proto) {
  if (opts.proxy_type == ConnectOptions::ProxyType::Socks5 &&
      !opts.proxy_host.empty() && proto == Protocol::Tcp) {
    return socks5_connect(opts.proxy_host, opts.proxy_port, host, port,
                          opts.proxy_user, opts.proxy_pass, opts.timeout_ms);
  }
  if (opts.proxy_type == ConnectOptions::ProxyType::HttpConnect &&
      !opts.proxy_host.empty() && proto == Protocol::Tcp) {
    return http_connect_proxy(opts.proxy_host, opts.proxy_port, host, port,
                              opts.timeout_ms);
  }
  return connect_direct(host, port, proto, opts.timeout_ms, opts.nodelay, opts.keepalive);
}

Result<Socket> listen_tcp(const std::string& host, std::uint16_t port, int backlog) {
#if LR_PLATFORM_WINDOWS
  auto ws = ensure_winsock();
  if (!ws) return Result<Socket>(Socket{-1}, ws.error_msg);
#endif
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  const std::string port_str = std::to_string(port);
  const char* node = (host.empty() || host == "0.0.0.0") ? nullptr : host.c_str();
  addrinfo* result = nullptr;
  const int rc = getaddrinfo(node, port_str.c_str(), &hints, &result);
  if (rc != 0 || !result)
    return Result<Socket>(Socket{-1}, std::string("listen getaddrinfo: ") + gai_strerror(rc));

  const int fd = static_cast<int>(
      ::socket(result->ai_family, result->ai_socktype, result->ai_protocol));
  if (fd < 0) {
    freeaddrinfo(result);
    return os_error("socket listen");
  }

  int yes = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));

  if (::bind(fd, result->ai_addr, static_cast<socklen_t_compat>(result->ai_addrlen)) < 0) {
    auto err = os_error("bind");
    freeaddrinfo(result);
    close_socket(fd);
    return Result<Socket>(Socket{-1}, err.error_msg);
  }
  freeaddrinfo(result);

  if (::listen(fd, backlog) < 0) {
    auto err = os_error("listen");
    close_socket(fd);
    return Result<Socket>(Socket{-1}, err.error_msg);
  }
  return Socket{fd};
}

Result<Socket> accept(Socket listener, int timeout_ms) {
  if (listener.fd < 0) return Result<Socket>(Socket{-1}, "invalid listener");

  if (timeout_ms >= 0) {
#if LR_PLATFORM_WINDOWS
    fd_set rset;
    FD_ZERO(&rset);
    FD_SET(static_cast<SOCKET>(listener.fd), &rset);
    timeval tv{};
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    const int rc = select(0, &rset, nullptr, nullptr, &tv);
    if (rc == 0) return Result<Socket>(Socket{-1}, "accept timed out");
    if (rc < 0) return os_error("select accept");
#else
    pollfd pfd{};
    pfd.fd = listener.fd;
    pfd.events = POLLIN;
    const int rc = poll(&pfd, 1, timeout_ms);
    if (rc == 0) return Result<Socket>(Socket{-1}, "accept timed out");
    if (rc < 0) return os_error("poll accept");
#endif
  }

  const int cfd = static_cast<int>(::accept(listener.fd, nullptr, nullptr));
  if (cfd < 0) return os_error("accept");
  return Socket{cfd};
}

Result<int> send(Socket sock, const std::uint8_t* data, int size) {
  if (sock.fd < 0) return Result<int>(-1, "invalid socket");
  if (size < 0) return Result<int>(-1, "negative size");
  if (size == 0) return 0;
  const int sent = static_cast<int>(
      ::send(sock.fd, reinterpret_cast<const char*>(data), size, 0));
  if (sent < 0) return os_error("send");
  return sent;
}

Result<void> send_all(Socket sock, const std::uint8_t* data, int size) {
  if (sock.fd < 0) return Result<void>("invalid socket");
  int off = 0;
  while (off < size) {
    auto r = send(sock, data + off, size - off);
    if (!r) return Result<void>(r.error_msg);
    if (*r == 0) return Result<void>("send returned 0");
    off += *r;
  }
  return Result<void>();
}

Result<int> recv(Socket sock, std::uint8_t* buffer, int buffer_size) {
  if (sock.fd < 0) return Result<int>(-1, "invalid socket");
  if (buffer_size <= 0) return Result<int>(-1, "invalid buffer size");
  const int received = static_cast<int>(
      ::recv(sock.fd, reinterpret_cast<char*>(buffer), buffer_size, 0));
  if (received < 0) return os_error("recv");
  return received;
}

Result<void> recv_exact(Socket sock, std::uint8_t* buffer, int size) {
  if (sock.fd < 0) return Result<void>("invalid socket");
  int off = 0;
  while (off < size) {
    auto r = recv(sock, buffer + off, size - off);
    if (!r) return Result<void>(r.error_msg);
    if (*r == 0) return Result<void>("peer closed during recv_exact");
    off += *r;
  }
  return Result<void>();
}

Result<int> sendto(Socket sock, const std::uint8_t* data, int size,
                    const std::string& host, std::uint16_t port) {
  if (sock.fd < 0) return Result<int>(-1, "invalid socket");
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  const std::string port_str = std::to_string(port);
  addrinfo* res = nullptr;
  if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res)
    return Result<int>(-1, "sendto DNS failed");
  const int n = static_cast<int>(::sendto(
      sock.fd, reinterpret_cast<const char*>(data), size, 0, res->ai_addr,
      static_cast<socklen_t_compat>(res->ai_addrlen)));
  freeaddrinfo(res);
  if (n < 0) return os_error("sendto");
  return n;
}

Result<int> recvfrom(Socket sock, std::uint8_t* buffer, int buffer_size,
                      std::string* out_host, std::uint16_t* out_port) {
  if (sock.fd < 0) return Result<int>(-1, "invalid socket");
  sockaddr_storage ss{};
  socklen_t_compat slen = sizeof(ss);
  const int n = static_cast<int>(::recvfrom(
      sock.fd, reinterpret_cast<char*>(buffer), buffer_size, 0,
      reinterpret_cast<sockaddr*>(&ss), &slen));
  if (n < 0) return os_error("recvfrom");
  if (out_host || out_port) {
    char hostbuf[NI_MAXHOST]{};
    char servbuf[NI_MAXSERV]{};
    if (getnameinfo(reinterpret_cast<sockaddr*>(&ss), slen, hostbuf, sizeof(hostbuf),
                    servbuf, sizeof(servbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0) {
      if (out_host) *out_host = hostbuf;
      if (out_port) *out_port = static_cast<std::uint16_t>(std::atoi(servbuf));
    }
  }
  return n;
}

Result<void> close(Socket sock) {
  if (sock.fd >= 0) close_socket(sock.fd);
  return Result<void>();
}

Result<void> set_timeout(Socket sock, int timeout_ms) {
  if (sock.fd < 0) return Result<void>("invalid socket");
#if LR_PLATFORM_WINDOWS
  DWORD tv = static_cast<DWORD>(timeout_ms < 0 ? 0 : timeout_ms);
  setsockopt(sock.fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv),
             sizeof(tv));
  setsockopt(sock.fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv),
             sizeof(tv));
#else
  timeval tv{};
  if (timeout_ms < 0) timeout_ms = 0;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  setsockopt(sock.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(sock.fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
  return Result<void>();
}

Result<void> set_nodelay(Socket sock, bool enabled) {
  if (sock.fd < 0) return Result<void>("invalid socket");
  int v = enabled ? 1 : 0;
  if (setsockopt(sock.fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&v),
                 sizeof(v)) != 0)
    return os_error("TCP_NODELAY");
  return Result<void>();
}

Result<void> set_keepalive(Socket sock, bool enabled) {
  if (sock.fd < 0) return Result<void>("invalid socket");
  int v = enabled ? 1 : 0;
  if (setsockopt(sock.fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&v),
                 sizeof(v)) != 0)
    return os_error("SO_KEEPALIVE");
  return Result<void>();
}

}  // namespace real::net
