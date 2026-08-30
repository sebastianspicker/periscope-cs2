// memory_helpers.cpp — Physical memory + pagemap helpers for Linux stack.

#include "real/linux/memory_internal.hpp"

#include <fstream>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#endif

namespace real::linux::mem {

uint64_t host_page_size() noexcept {
#if LR_PLATFORM_LINUX
    long ps = ::sysconf(_SC_PAGESIZE);
    return ps > 0 ? static_cast<uint64_t>(ps) : pagemap::kDefaultPageSz;
#else
    return pagemap::kDefaultPageSz;
#endif
}

// ── Physical: /dev/mem ─────────────────────────────────────────────
// SCAR: open(/dev/mem) + mmap; visible to audit, LSM, rootkit hunters.
// BLUE: watch for CAP_SYS_RAWIO holders opening mem devices.
// MITIGATION: CONFIG_STRICT_DEVMEM, lockdown LSM, IOMMU.

Result<std::vector<uint8_t>> read_physical_devmem(uint64_t phys_addr,
                                                  size_t size) noexcept {
    if (!phys_request_valid(phys_addr, size)) {
        return Result<std::vector<uint8_t>>({}, "invalid physical read size");
    }
#if LR_PLATFORM_LINUX
    int fd = detail::open_safe("/dev/mem", O_RDONLY | O_SYNC);
    if (fd < 0) {
        return Result<std::vector<uint8_t>>({}, "Cannot open /dev/mem");
    }
    const uint64_t page_size = host_page_size();
    const uint64_t page_start = phys_addr & ~(page_size - 1);
    const size_t offset = static_cast<size_t>(phys_addr - page_start);
    const size_t map_size =
        ((offset + size + static_cast<size_t>(page_size) - 1) /
         static_cast<size_t>(page_size)) *
        static_cast<size_t>(page_size);

    void* map = ::mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd,
                       static_cast<off_t>(page_start));
    if (map == MAP_FAILED) {
        // Some kernels allow pread on /dev/mem even when mmap fails.
        auto via_pread = detail::pread_all(fd, phys_addr, size);
        ::close(fd);
        return via_pread;
    }
    std::vector<uint8_t> result(size);
    std::memcpy(result.data(), static_cast<uint8_t*>(map) + offset, size);
    ::munmap(map, map_size);
    ::close(fd);
    return result;
#else
    (void)phys_addr;
    (void)size;
    return Result<std::vector<uint8_t>>({}, "read_physical_devmem requires Linux");
#endif
}

Result<std::vector<uint8_t>> read_physical_crash(uint64_t phys_addr,
                                                 size_t size) noexcept {
    if (!phys_request_valid(phys_addr, size)) {
        return Result<std::vector<uint8_t>>({}, "invalid physical read size");
    }
#if LR_PLATFORM_LINUX
    int fd = detail::open_safe("/dev/crash", O_RDONLY);
    if (fd < 0) {
        return Result<std::vector<uint8_t>>(
            {}, "Cannot access physical memory: /dev/crash unavailable");
    }
    auto r = detail::pread_all(fd, phys_addr, size);
    ::close(fd);
    return r;
#else
    (void)phys_addr;
    (void)size;
    return Result<std::vector<uint8_t>>({}, "read_physical_crash requires Linux");
#endif
}

Result<std::vector<uint8_t>> read_physical(uint64_t phys_addr,
                                           size_t size) noexcept {
    auto r = read_physical_devmem(phys_addr, size);
    if (r) return r;
    return read_physical_crash(phys_addr, size);
}

Result<void> write_physical(uint64_t phys_addr,
                            const std::vector<uint8_t>& data) noexcept {
    if (!phys_request_valid(phys_addr, data.size())) {
        return Result<void>("invalid physical write size");
    }
#if LR_PLATFORM_LINUX
    std::printf("[linux:mem] write_physical phys=0x%llx size=%zu (DANGEROUS)\n",
                (unsigned long long)phys_addr, data.size());
    int fd = detail::open_safe("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) return Result<void>("Cannot open /dev/mem for writing");

    const uint64_t page_size = host_page_size();
    const uint64_t page_start = phys_addr & ~(page_size - 1);
    const size_t offset = static_cast<size_t>(phys_addr - page_start);
    const size_t map_size =
        ((offset + data.size() + static_cast<size_t>(page_size) - 1) /
         static_cast<size_t>(page_size)) *
        static_cast<size_t>(page_size);

    void* map = ::mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                       static_cast<off_t>(page_start));
    if (map == MAP_FAILED) {
        ::close(fd);
        return Result<void>("mmap /dev/mem for write failed");
    }
    std::memcpy(static_cast<uint8_t*>(map) + offset, data.data(), data.size());
    ::munmap(map, map_size);
    ::close(fd);
    return Result<void>();
#else
    (void)phys_addr;
    (void)data;
    return detail::not_linux("write_physical");
#endif
}

// ── Pagemap ────────────────────────────────────────────────────────

Result<pagemap::Entry> read_pagemap_entry(uint32_t pid,
                                          uint64_t virt_addr) noexcept {
#if LR_PLATFORM_LINUX
    char path[64];
    if (pid == 0) {
        std::snprintf(path, sizeof(path), "/proc/self/pagemap");
    } else {
        std::snprintf(path, sizeof(path), "/proc/%u/pagemap", pid);
    }
    int fd = detail::open_safe(path, O_RDONLY);
    if (fd < 0) {
        return Result<pagemap::Entry>({}, std::string("Cannot open ") + path);
    }
    const uint64_t page_size = host_page_size();
    const off_t off =
        static_cast<off_t>(pagemap::entry_file_offset(virt_addr, page_size));
    uint64_t raw = 0;
    ssize_t n = ::pread(fd, &raw, sizeof(raw), off);
    ::close(fd);
    if (n != static_cast<ssize_t>(sizeof(raw))) {
        return Result<pagemap::Entry>({}, "pagemap read failed");
    }
    return pagemap::decode(raw);
#else
    (void)pid;
    (void)virt_addr;
    return Result<pagemap::Entry>({}, "read_pagemap_entry requires Linux");
#endif
}

Result<uint64_t> virtual_to_physical(uint64_t virt_addr) noexcept {
    auto e = read_pagemap_entry(0, virt_addr);
    if (!e) return Result<uint64_t>(0, e.error_msg);
    if (!e->present) return Result<uint64_t>(0, "Page not present in pagemap");
    return pagemap::to_physical(*e, virt_addr, host_page_size());
}

Result<uint64_t> process_virtual_to_physical(uint32_t pid,
                                             uint64_t virt_addr) noexcept {
    auto e = read_pagemap_entry(pid, virt_addr);
    if (!e) return Result<uint64_t>(0, e.error_msg);
    if (!e->present) return Result<uint64_t>(0, "Page not present");
    return pagemap::to_physical(*e, virt_addr, host_page_size());
}

} // namespace real::linux::mem
