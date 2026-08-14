// memory.cpp — Linux process/virtual memory access and capability probes.

#include "real/linux/memory_internal.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/ptrace.h>
#  include <sys/syscall.h>
#  include <sys/types.h>
#  include <sys/uio.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace real::linux::mem {

// ── process_vm_readv / writev ──────────────────────────────────────
// SCAR: process_vm_* leaves no /proc handle but is LSM-audited; requires
//       ptrace-like permissions (same UID or CAP_SYS_PTRACE).
// BLUE: Yama ptrace_scope, seccomp, audit SYSCALL records.
// MITIGATION: Yama scope=1+, SELinux domain transition.

Result<std::vector<uint8_t>> read_process_memory(uint32_t pid, uint64_t addr,
                                                 size_t size) noexcept {
    if (!virt_request_valid(addr, size)) {
        return Result<std::vector<uint8_t>>({}, "invalid virtual read size");
    }
#if LR_PLATFORM_LINUX
    std::vector<uint8_t> buf(size);
    struct iovec local {};
    struct iovec remote {};
    local.iov_base = buf.data();
    local.iov_len = size;
    remote.iov_base = reinterpret_cast<void*>(static_cast<uintptr_t>(addr));
    remote.iov_len = size;

    ssize_t bytes = ::process_vm_readv(static_cast<pid_t>(pid), &local, 1,
                                       &remote, 1, 0);
    if (bytes > 0 && static_cast<size_t>(bytes) == size) {
        return buf;
    }
    // Fallback chain: procfs → ptrace
    auto via_proc = read_process_mem_procfs(pid, addr, size);
    if (via_proc) return via_proc;
    return read_process_memory_ptrace(pid, addr, size);
#else
    (void)pid;
    return Result<std::vector<uint8_t>>({}, "read_process_memory requires Linux");
#endif
}

Result<void> write_process_memory(uint32_t pid, uint64_t addr,
                                  const std::vector<uint8_t>& data) noexcept {
    if (!virt_request_valid(addr, data.size())) {
        return Result<void>("invalid virtual write size");
    }
#if LR_PLATFORM_LINUX
    struct iovec local {};
    struct iovec remote {};
    local.iov_base = const_cast<uint8_t*>(data.data());
    local.iov_len = data.size();
    remote.iov_base = reinterpret_cast<void*>(static_cast<uintptr_t>(addr));
    remote.iov_len = data.size();
    ssize_t bytes = ::process_vm_writev(static_cast<pid_t>(pid), &local, 1,
                                        &remote, 1, 0);
    if (bytes > 0 && static_cast<size_t>(bytes) == data.size()) {
        return Result<void>();
    }
    auto via_proc = write_process_mem_procfs(pid, addr, data);
    if (via_proc) return via_proc;
    return write_process_memory_ptrace(pid, addr, data);
#else
    (void)pid;
    (void)addr;
    (void)data;
    return detail::not_linux("write_process_memory");
#endif
}

Result<std::vector<uint8_t>> read_process_mem_procfs(uint32_t pid, uint64_t addr,
                                                     size_t size) noexcept {
    if (!virt_request_valid(addr, size)) {
        return Result<std::vector<uint8_t>>({}, "invalid virtual read size");
    }
#if LR_PLATFORM_LINUX
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%u/mem", pid);
    int fd = detail::open_safe(path, O_RDONLY);
    if (fd < 0) {
        return Result<std::vector<uint8_t>>({},
            std::string("Cannot open ") + path);
    }
    auto r = detail::pread_all(fd, addr, size);
    ::close(fd);
    return r;
#else
    (void)pid;
    return Result<std::vector<uint8_t>>({},
        "read_process_mem_procfs requires Linux");
#endif
}

Result<void> write_process_mem_procfs(uint32_t pid, uint64_t addr,
                                      const std::vector<uint8_t>& data) noexcept {
#if LR_PLATFORM_LINUX
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%u/mem", pid);
    int fd = detail::open_safe(path, O_RDWR);
    if (fd < 0) return Result<void>(std::string("Cannot open ") + path);
    size_t done = 0;
    while (done < data.size()) {
        ssize_t n = ::pwrite(fd, data.data() + done, data.size() - done,
                             static_cast<off_t>(addr + done));
        if (n < 0) {
            ::close(fd);
            return Result<void>(std::string("pwrite failed: ") +
                                std::strerror(errno));
        }
        if (n == 0) break;
        done += static_cast<size_t>(n);
    }
    ::close(fd);
    if (done != data.size()) return Result<void>("short pwrite to /proc/pid/mem");
    return Result<void>();
#else
    (void)pid;
    (void)addr;
    (void)data;
    return detail::not_linux("write_process_mem_procfs");
#endif
}

// ── ptrace PEEKDATA / POKEDATA ─────────────────────────────────────
// SCAR: PTRACE_ATTACH stops the target; visible in TracerPid; signal stops.
// BLUE: TracerPid monitoring, ptrace_scope, process_vm preference detection.

Result<std::vector<uint8_t>> read_process_memory_ptrace(uint32_t pid,
                                                        uint64_t addr,
                                                        size_t size) noexcept {
#if LR_PLATFORM_LINUX
    if (::ptrace(PTRACE_ATTACH, static_cast<pid_t>(pid), nullptr, nullptr) < 0) {
        return Result<std::vector<uint8_t>>(
            {}, std::string("PTRACE_ATTACH failed: ") + std::strerror(errno));
    }
    int status = 0;
    if (::waitpid(static_cast<pid_t>(pid), &status, 0) < 0) {
        ::ptrace(PTRACE_DETACH, static_cast<pid_t>(pid), nullptr, nullptr);
        return Result<std::vector<uint8_t>>({}, "waitpid after attach failed");
    }

    std::vector<uint8_t> buf(size);
    // ptrace word size
    constexpr size_t kWord = sizeof(long);
    for (size_t i = 0; i < size; i += kWord) {
        errno = 0;
        long word = ::ptrace(PTRACE_PEEKDATA, static_cast<pid_t>(pid),
                             reinterpret_cast<void*>(addr + i), nullptr);
        if (word == -1 && errno != 0) {
            ::ptrace(PTRACE_DETACH, static_cast<pid_t>(pid), nullptr, nullptr);
            return Result<std::vector<uint8_t>>(
                {}, std::string("PTRACE_PEEKDATA failed: ") + std::strerror(errno));
        }
        size_t chunk = std::min(kWord, size - i);
        std::memcpy(buf.data() + i, &word, chunk);
    }
    ::ptrace(PTRACE_DETACH, static_cast<pid_t>(pid), nullptr, nullptr);
    return buf;
#else
    (void)pid;
    (void)addr;
    (void)size;
    return Result<std::vector<uint8_t>>({},
        "read_process_memory_ptrace requires Linux");
#endif
}

Result<void> write_process_memory_ptrace(
    uint32_t pid, uint64_t addr, const std::vector<uint8_t>& data) noexcept {
#if LR_PLATFORM_LINUX
    if (::ptrace(PTRACE_ATTACH, static_cast<pid_t>(pid), nullptr, nullptr) < 0) {
        return Result<void>(std::string("PTRACE_ATTACH failed: ") +
                            std::strerror(errno));
    }
    int status = 0;
    ::waitpid(static_cast<pid_t>(pid), &status, 0);

    constexpr size_t kWord = sizeof(long);
    for (size_t i = 0; i < data.size(); i += kWord) {
        long word = 0;
        size_t chunk = std::min(kWord, data.size() - i);
        if (chunk < kWord) {
            errno = 0;
            word = ::ptrace(PTRACE_PEEKDATA, static_cast<pid_t>(pid),
                            reinterpret_cast<void*>(addr + i), nullptr);
            if (word == -1 && errno != 0) {
                ::ptrace(PTRACE_DETACH, static_cast<pid_t>(pid), nullptr, nullptr);
                return Result<void>("PTRACE_PEEKDATA (RMW) failed");
            }
        }
        std::memcpy(&word, data.data() + i, chunk);
        if (::ptrace(PTRACE_POKEDATA, static_cast<pid_t>(pid),
                     reinterpret_cast<void*>(addr + i),
                     reinterpret_cast<void*>(word)) < 0) {
            ::ptrace(PTRACE_DETACH, static_cast<pid_t>(pid), nullptr, nullptr);
            return Result<void>(std::string("PTRACE_POKEDATA failed: ") +
                                std::strerror(errno));
        }
    }
    ::ptrace(PTRACE_DETACH, static_cast<pid_t>(pid), nullptr, nullptr);
    return Result<void>();
#else
    (void)pid;
    (void)addr;
    (void)data;
    return detail::not_linux("write_process_memory_ptrace");
#endif
}

// ── /proc/kcore ────────────────────────────────────────────────────

Result<std::vector<uint8_t>> read_kernel_memory(uint64_t addr,
                                                size_t size) noexcept {
    if (!virt_request_valid(addr, size)) {
        return Result<std::vector<uint8_t>>({}, "invalid kernel read size");
    }
#if LR_PLATFORM_LINUX
    int fd = detail::open_safe("/proc/kcore", O_RDONLY);
    if (fd < 0) {
        return Result<std::vector<uint8_t>>({}, "Cannot open /proc/kcore");
    }
    auto r = detail::pread_all(fd, addr, size);
    ::close(fd);
    return r;
#else
    (void)addr;
    return Result<std::vector<uint8_t>>({}, "read_kernel_memory requires Linux");
#endif
}

Result<std::vector<uint8_t>> read_self_memory(uint64_t addr,
                                              size_t size) noexcept {
    if (!virt_request_valid(addr, size)) {
        return Result<std::vector<uint8_t>>({}, "invalid self read size");
    }
    // Direct memcpy from own address space — no syscall scar for foreign PID.
    // Still faults if addr is invalid (we catch via mincore/probe when possible).
#if LR_PLATFORM_LINUX
    std::vector<uint8_t> buf(size);
    // Use process_vm_readv on self for consistent permission checks.
    struct iovec local {};
    struct iovec remote {};
    local.iov_base = buf.data();
    local.iov_len = size;
    remote.iov_base = reinterpret_cast<void*>(static_cast<uintptr_t>(addr));
    remote.iov_len = size;
    ssize_t n = ::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0);
    if (n < 0) {
        // Fallback: volatile memcpy (may SIGSEGV — caller must pass valid addr)
        std::memcpy(buf.data(), reinterpret_cast<const void*>(addr), size);
        return buf;
    }
    buf.resize(static_cast<size_t>(n));
    return buf;
#else
    // Host unit-test path: allow reading from a local buffer address when
    // the pointer is in our process (always true on Windows for test stack).
    std::vector<uint8_t> buf(size);
    std::memcpy(buf.data(), reinterpret_cast<const void*>(
                                static_cast<uintptr_t>(addr)), size);
    return buf;
#endif
}

// ── Capability probes ──────────────────────────────────────────────

Result<bool> devmem_accessible() noexcept {
#if LR_PLATFORM_LINUX
    int fd = ::open("/dev/mem", O_RDONLY | O_SYNC);
    if (fd >= 0) {
        ::close(fd);
        return true;
    }
    return Result<bool>(false, "/dev/mem not accessible");
#else
    return Result<bool>(false, "not Linux");
#endif
}

Result<bool> kmod_available() noexcept {
#if LR_PLATFORM_LINUX
    long ret = ::syscall(__NR_finit_module, -1, "", 0);
    if (ret < 0 && errno != EBADF && errno != ENOENT) {
        // EPERM / EACCES => blocked; ENOSYS => missing
        if (errno == ENOSYS) {
            return Result<bool>(false, "finit_module unavailable");
        }
        if (errno == EPERM || errno == EACCES) {
            return Result<bool>(false, "finit_module permission denied");
        }
    }
    return true;
#else
    return Result<bool>(false, "not Linux");
#endif
}

Result<bool> process_vm_available() noexcept {
#if LR_PLATFORM_LINUX
    uint8_t probe = 0xA5;
    uint8_t out = 0;
    struct iovec local {&out, 1};
    struct iovec remote {&probe, 1};
    ssize_t n = ::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0);
    return n == 1 && out == 0xA5;
#else
    return Result<bool>(false, "not Linux");
#endif
}

Result<bool> ptrace_available() noexcept {
#if LR_PLATFORM_LINUX
    // Non-destructive: check Yama scope only (0 = classic ptrace allowed).
    std::ifstream f("/proc/sys/kernel/yama/ptrace_scope");
    if (!f) return true; // no Yama → classic rules
    int scope = 1;
    f >> scope;
    return scope <= 1;
#else
    return Result<bool>(false, "not Linux");
#endif
}

Result<bool> kcore_accessible() noexcept {
#if LR_PLATFORM_LINUX
    int fd = ::open("/proc/kcore", O_RDONLY);
    if (fd >= 0) {
        ::close(fd);
        return true;
    }
    return Result<bool>(false, "/proc/kcore not accessible");
#else
    return Result<bool>(false, "not Linux");
#endif
}

// ── Module load ────────────────────────────────────────────────────

Result<void> load_kernel_module(const std::vector<uint8_t>& elf_data,
                                const std::string& params) noexcept {
#if LR_PLATFORM_LINUX
    if (elf_data.empty()) return Result<void>("empty module image");
    int fd = static_cast<int>(::syscall(__NR_memfd_create, "ac_lab_module", 0));
    if (fd < 0) return Result<void>("memfd_create failed");

    size_t off = 0;
    while (off < elf_data.size()) {
        ssize_t n = ::write(fd, elf_data.data() + off, elf_data.size() - off);
        if (n < 0) {
            ::close(fd);
            return Result<void>("write memfd failed");
        }
        off += static_cast<size_t>(n);
    }
    const char* mod_params = params.empty() ? "" : params.c_str();
    long ret = ::syscall(__NR_finit_module, fd, mod_params, 0);
    ::close(fd);
    if (ret < 0) {
        return Result<void>(std::string("finit_module failed: ") +
                            std::strerror(errno));
    }
    return Result<void>();
#else
    (void)elf_data;
    (void)params;
    return detail::not_linux("load_kernel_module");
#endif
}

Result<void> load_kernel_module_path(const std::string& path,
                                     const std::string& params) noexcept {
#if LR_PLATFORM_LINUX
    int fd = detail::open_safe(path.c_str(), O_RDONLY);
    if (fd < 0) return Result<void>("Cannot open module file");
    const char* mod_params = params.empty() ? "" : params.c_str();
    long ret = ::syscall(__NR_finit_module, fd, mod_params, 0);
    ::close(fd);
    if (ret < 0) {
        return Result<void>(std::string("finit_module failed: ") +
                            std::strerror(errno));
    }
    return Result<void>();
#else
    (void)path;
    (void)params;
    return detail::not_linux("load_kernel_module_path");
#endif
}

Result<void> unload_kernel_module(const std::string& module_name) noexcept {
#if LR_PLATFORM_LINUX
    long ret = ::syscall(__NR_delete_module, module_name.c_str(), 0);
    if (ret < 0) {
        if (errno == ENOENT) {
            return Result<void>("Module not loaded: " + module_name);
        }
        return Result<void>(std::string("delete_module failed: ") +
                            std::strerror(errno));
    }
    return Result<void>();
#else
    (void)module_name;
    return detail::not_linux("unload_kernel_module");
#endif
}

} // namespace real::linux::mem
