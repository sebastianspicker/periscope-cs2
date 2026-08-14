// kmod_client.cpp — Userspace library for /dev/aclab IOCTLs.

#include "real/linux/kmod_client.hpp"
#include "real/platform.hpp"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

namespace real::linux::kmod {

Client::~Client() { close(); }

Client::Client(Client&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }

Client& Client::operator=(Client&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

Result<void> Client::open(const std::string& path) noexcept {
#if LR_PLATFORM_LINUX
    close();
    fd_ = ::open(path.c_str(), O_RDWR);
    if (fd_ < 0) {
        return Result<void>("Cannot open " + path +
                            " (load module: insmod aclab_module.ko)");
    }
    return Result<void>();
#else
    (void)path;
    return Result<void>("kmod::Client::open requires Linux");
#endif
}

void Client::close() noexcept {
#if LR_PLATFORM_LINUX
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#else
    fd_ = -1;
#endif
}

Result<std::vector<uint8_t>> Client::phys_read(uint64_t phys,
                                               size_t size) noexcept {
    if (!phys_size_ok(size)) {
        return Result<std::vector<uint8_t>>({}, "size exceeds ACLAB_MAX_XFER");
    }
#if LR_PLATFORM_LINUX
    if (fd_ < 0) return Result<std::vector<uint8_t>>({}, "device not open");
    aclab_phys_op op {};
    op.phys_addr = phys;
    op.size = static_cast<uint32_t>(size);
    if (::ioctl(fd_, ACLAB_IOCTL_PHYS_READ, &op) < 0) {
        return Result<std::vector<uint8_t>>({}, "PHYS_READ ioctl failed");
    }
    return std::vector<uint8_t>(op.data, op.data + size);
#else
    (void)phys;
    return Result<std::vector<uint8_t>>({}, "phys_read requires Linux");
#endif
}

Result<void> Client::phys_write(uint64_t phys,
                                const std::vector<uint8_t>& data) noexcept {
    if (!phys_size_ok(data.size())) return Result<void>("size exceeds ACLAB_MAX_XFER");
#if LR_PLATFORM_LINUX
    if (fd_ < 0) return Result<void>("device not open");
    aclab_phys_op op {};
    op.phys_addr = phys;
    op.size = static_cast<uint32_t>(data.size());
    std::memcpy(op.data, data.data(), data.size());
    if (::ioctl(fd_, ACLAB_IOCTL_PHYS_WRITE, &op) < 0) {
        return Result<void>("PHYS_WRITE ioctl failed");
    }
    return Result<void>();
#else
    (void)phys;
    (void)data;
    return Result<void>("phys_write requires Linux");
#endif
}

Result<uint64_t> Client::get_cr3(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    if (fd_ < 0) return Result<uint64_t>(0, "device not open");
    aclab_cr3_req req {};
    req.pid = static_cast<pid_t>(pid);
    if (::ioctl(fd_, ACLAB_IOCTL_GET_CR3, &req) < 0) {
        return Result<uint64_t>(0, "GET_CR3 ioctl failed");
    }
    return req.cr3;
#else
    (void)pid;
    return Result<uint64_t>(0, "get_cr3 requires Linux");
#endif
}

Result<std::vector<uint8_t>> Client::virt_read(uint32_t pid, uint64_t addr,
                                               size_t size) noexcept {
    if (!virt_size_ok(size)) {
        return Result<std::vector<uint8_t>>({}, "size exceeds ACLAB_MAX_XFER");
    }
#if LR_PLATFORM_LINUX
    if (fd_ < 0) return Result<std::vector<uint8_t>>({}, "device not open");
    aclab_virt_op op {};
    op.pid = static_cast<pid_t>(pid);
    op.virt_addr = addr;
    op.size = static_cast<uint32_t>(size);
    if (::ioctl(fd_, ACLAB_IOCTL_VIRT_READ, &op) < 0) {
        return Result<std::vector<uint8_t>>({}, "VIRT_READ ioctl failed");
    }
    return std::vector<uint8_t>(op.data, op.data + size);
#else
    (void)pid;
    (void)addr;
    return Result<std::vector<uint8_t>>({}, "virt_read requires Linux");
#endif
}

Result<void> Client::virt_write(uint32_t pid, uint64_t addr,
                                const std::vector<uint8_t>& data) noexcept {
    if (!virt_size_ok(data.size())) return Result<void>("size exceeds ACLAB_MAX_XFER");
#if LR_PLATFORM_LINUX
    if (fd_ < 0) return Result<void>("device not open");
    aclab_virt_op op {};
    op.pid = static_cast<pid_t>(pid);
    op.virt_addr = addr;
    op.size = static_cast<uint32_t>(data.size());
    std::memcpy(op.data, data.data(), data.size());
    if (::ioctl(fd_, ACLAB_IOCTL_VIRT_WRITE, &op) < 0) {
        return Result<void>("VIRT_WRITE ioctl failed");
    }
    return Result<void>();
#else
    (void)pid;
    (void)addr;
    (void)data;
    return Result<void>("virt_write requires Linux");
#endif
}

Result<void> Client::hide_process(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    if (fd_ < 0) return Result<void>("device not open");
    pid_t p = static_cast<pid_t>(pid);
    if (::ioctl(fd_, ACLAB_IOCTL_HIDE_PROC, &p) < 0) {
        return Result<void>("HIDE_PROC ioctl failed");
    }
    return Result<void>();
#else
    (void)pid;
    return Result<void>("hide_process requires Linux");
#endif
}

Result<void> Client::steal_cred(uint32_t target_pid,
                                uint32_t source_pid) noexcept {
#if LR_PLATFORM_LINUX
    if (fd_ < 0) return Result<void>("device not open");
    aclab_cred_steal cs {};
    cs.target_pid = static_cast<pid_t>(target_pid);
    cs.source_pid = static_cast<pid_t>(source_pid);
    if (::ioctl(fd_, ACLAB_IOCTL_STEAL_CRED, &cs) < 0) {
        return Result<void>("STEAL_CRED ioctl failed");
    }
    return Result<void>();
#else
    (void)target_pid;
    (void)source_pid;
    return Result<void>("steal_cred requires Linux");
#endif
}

Result<ModuleInfo> Client::info() noexcept {
#if LR_PLATFORM_LINUX
    if (fd_ < 0) return Result<ModuleInfo>({}, "device not open");
    aclab_mod_info mi {};
    if (::ioctl(fd_, ACLAB_IOCTL_MOD_INFO, &mi) < 0) {
        // Older modules may lack MOD_INFO — soft-fail with defaults.
        ModuleInfo fallback;
        fallback.abi_version = 0;
        fallback.build_tag = "unknown";
        return Result<ModuleInfo>(fallback, "MOD_INFO unsupported");
    }
    ModuleInfo out;
    out.major = mi.major;
    out.minor = mi.minor;
    out.abi_version = mi.abi_version;
    out.build_tag.assign(mi.build_tag,
                         strnlen(mi.build_tag, sizeof(mi.build_tag)));
    return out;
#else
    return Result<ModuleInfo>({}, "info requires Linux");
#endif
}

Result<bool> device_available(const std::string& path) noexcept {
#if LR_PLATFORM_LINUX
    int fd = ::open(path.c_str(), O_RDWR);
    if (fd < 0) return Result<bool>(false, "device not openable");
    ::close(fd);
    return true;
#else
    (void)path;
    return Result<bool>(false, "not Linux");
#endif
}

} // namespace real::linux::kmod
