// memory_internal.hpp — Shared Linux memory helpers for multi-TU split.
#pragma once

#include "real/linux/memory.hpp"
#include "real/platform.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace real::linux::mem {
namespace detail {

#if LR_PLATFORM_LINUX
inline int open_safe(const char* path, int flags, int mode = 0) {
    int fd = mode ? ::open(path, flags, mode) : ::open(path, flags);
    if (fd < 0) {
        std::printf("[linux:mem] open(%s) failed errno=%d\n", path, errno);
    }
    return fd;
}

inline Result<std::vector<uint8_t>> pread_all(int fd, uint64_t offset, size_t size) {
    std::vector<uint8_t> buf(size);
    size_t got = 0;
    while (got < size) {
        ssize_t n = ::pread(fd, buf.data() + got, size - got,
                            static_cast<off_t>(offset + got));
        if (n < 0) {
            if (errno == EINTR) continue;
            return Result<std::vector<uint8_t>>({},
                std::string("pread failed: ") + std::strerror(errno));
        }
        if (n == 0) break;
        got += static_cast<size_t>(n);
    }
    if (got != size) {
        buf.resize(got);
        if (got == 0) {
            return Result<std::vector<uint8_t>>({}, "pread returned 0 bytes");
        }
    }
    return buf;
}
#endif

inline Result<void> not_linux(const char* op) {
    return Result<void>(std::string(op) + " requires Linux");
}

}  // namespace detail
}  // namespace real::linux::mem
