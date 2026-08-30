// kmod_client.hpp — Userspace client library for /dev/aclab.
//
// Wraps all ACLAB IOCTLs. SCAR: open of /dev/aclab is visible in lsof,
// /proc/self/fd, audit. IOCTLs logged by the module to dmesg.

#pragma once

#include "real/error.hpp"
#include "real/linux/aclab_ioctl.h"

#include <cstdint>
#include <string>
#include <vector>

namespace real::linux::kmod {

struct ModuleInfo {
    uint32_t major = 0;
    uint32_t minor = 0;
    uint32_t abi_version = 0;
    std::string build_tag;
};

/// RAII handle to /dev/aclab.
class Client {
public:
    Client() = default;
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&& other) noexcept;
    Client& operator=(Client&& other) noexcept;

    Result<void> open(const std::string& path = ACLAB_DEV_PATH) noexcept;
    void close() noexcept;
    bool is_open() const noexcept { return fd_ >= 0; }
    int native_fd() const noexcept { return fd_; }

    Result<std::vector<uint8_t>> phys_read(uint64_t phys, size_t size) noexcept;
    Result<void> phys_write(uint64_t phys,
                            const std::vector<uint8_t>& data) noexcept;
    Result<uint64_t> get_cr3(uint32_t pid) noexcept;
    Result<std::vector<uint8_t>> virt_read(uint32_t pid, uint64_t addr,
                                           size_t size) noexcept;
    Result<void> virt_write(uint32_t pid, uint64_t addr,
                            const std::vector<uint8_t>& data) noexcept;
    Result<void> hide_process(uint32_t pid) noexcept;
    Result<void> steal_cred(uint32_t target_pid, uint32_t source_pid) noexcept;
    Result<ModuleInfo> info() noexcept;

private:
    int fd_ = -1;
};

/// Probe whether /dev/aclab exists and is openable.
Result<bool> device_available(const std::string& path = ACLAB_DEV_PATH) noexcept;

/// Pure: validate phys op size against ABI.
inline bool phys_size_ok(size_t size) noexcept {
    return size > 0 && size <= ACLAB_MAX_XFER;
}

/// Pure: validate virt op size against ABI.
inline bool virt_size_ok(size_t size) noexcept {
    return size > 0 && size <= ACLAB_MAX_XFER;
}

/// Pure: check ABI version compatibility.
inline bool abi_compatible(uint32_t remote) noexcept {
    return remote == ACLAB_ABI_VERSION;
}

} // namespace real::linux::kmod
