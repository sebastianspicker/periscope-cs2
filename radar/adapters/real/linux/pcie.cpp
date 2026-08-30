// pcie.cpp — Linux PCIe sysfs enumeration, BAR mmap, config, port I/O.

#include "real/linux/pcie.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/io.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace real::linux::pcie {
namespace {

uint64_t read_sysfs_hex(const std::string& path) {
    std::ifstream f(path);
    if (!f) return 0;
    uint64_t val = 0;
    f >> std::hex >> val;
    return val;
}

std::string readlink_basename(const std::string& path) {
#if LR_PLATFORM_LINUX
    char target[512] = {};
    ssize_t len = ::readlink(path.c_str(), target, sizeof(target) - 1);
    if (len <= 0) return {};
    target[len] = '\0';
    const char* slash = std::strrchr(target, '/');
    return slash ? std::string(slash + 1) : std::string(target);
#else
    (void)path;
    return {};
#endif
}

} // namespace

void finalize_device(PciDevice& dev) noexcept {
    dev.bus = dev.bdf.bus;
    dev.device = dev.bdf.device;
    dev.function = dev.bdf.function;
    if (dev.bars[0].valid) {
        dev.bar0 = dev.bars[0].start;
        dev.bar0_size = dev.bars[0].size;
    }
    if (dev.bars[1].valid) {
        dev.bar1 = dev.bars[1].start;
        dev.bar1_size = dev.bars[1].size;
    }
    dev.is_dma_capable = pci::is_dma_capable(dev.bars.data(), dev.bars.size());
}

Result<std::vector<PciDevice>> enumerate() noexcept {
#if LR_PLATFORM_LINUX
    std::vector<PciDevice> devices;
    DIR* dir = ::opendir("/sys/bus/pci/devices/");
    if (!dir) {
        return Result<std::vector<PciDevice>>(
            {}, "Cannot open /sys/bus/pci/devices/");
    }

    while (dirent* entry = ::readdir(dir)) {
        if (entry->d_name[0] == '.') continue;
        pci::Bdf bdf;
        if (!pci::parse_bdf(entry->d_name, bdf)) continue;

        PciDevice dev;
        dev.bdf = bdf;
        dev.sysfs_path =
            std::string("/sys/bus/pci/devices/") + entry->d_name;

        dev.vendor_id = static_cast<uint16_t>(
            read_sysfs_hex(dev.sysfs_path + "/vendor"));
        dev.device_id = static_cast<uint16_t>(
            read_sysfs_hex(dev.sysfs_path + "/device"));
        uint64_t cls = read_sysfs_hex(dev.sysfs_path + "/class");
        dev.class_code = static_cast<uint8_t>((cls >> 16) & 0xFF);
        dev.subclass = static_cast<uint8_t>((cls >> 8) & 0xFF);
        dev.prog_if = static_cast<uint8_t>(cls & 0xFF);
        dev.revision = static_cast<uint8_t>(
            read_sysfs_hex(dev.sysfs_path + "/revision"));

        struct stat st {};
        std::string driver_path = dev.sysfs_path + "/driver";
        if (::lstat(driver_path.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
            dev.driver = readlink_basename(driver_path);
        }

        std::ifstream res(dev.sysfs_path + "/resource");
        if (res) {
            std::string line;
            for (int bar = 0; bar < 6 && std::getline(res, line); ++bar) {
                pci::parse_resource_line(line, dev.bars[static_cast<size_t>(bar)]);
            }
        }

        dev.iommu_group = readlink_basename(dev.sysfs_path + "/iommu_group");
        {
            struct stat st2 {};
            if (::stat((dev.sysfs_path + "/sriov_numvfs").c_str(), &st2) == 0) {
                dev.sriov_capable = true;
            }
        }

        finalize_device(dev);
        devices.push_back(std::move(dev));
    }
    ::closedir(dir);
    std::printf("[linux:pcie] Enumerated %zu devices\n", devices.size());
    return devices;
#else
    return Result<std::vector<PciDevice>>({}, "enumerate requires Linux");
#endif
}

Result<PciDevice> find(uint16_t vendor_id, uint16_t device_id) noexcept {
    auto devices = enumerate();
    if (!devices) return Result<PciDevice>({}, devices.error_msg);
    for (const auto& dev : *devices) {
        if (dev.vendor_id == vendor_id && dev.device_id == device_id) return dev;
    }
    return Result<PciDevice>(
        {}, "PCI device " + std::to_string(vendor_id) + ":" +
                std::to_string(device_id) + " not found");
}

Result<PciDevice> find_bdf(const pci::Bdf& bdf) noexcept {
    auto devices = enumerate();
    if (!devices) return Result<PciDevice>({}, devices.error_msg);
    for (const auto& dev : *devices) {
        if (dev.bdf.domain == bdf.domain && dev.bdf.bus == bdf.bus &&
            dev.bdf.device == bdf.device && dev.bdf.function == bdf.function) {
            return dev;
        }
    }
    return Result<PciDevice>({}, "BDF not found: " + pci::format_bdf(bdf));
}

Result<std::vector<uint8_t>> bar_read(const PciDevice& dev, unsigned bar_index,
                                      uint64_t offset, size_t size) noexcept {
#if LR_PLATFORM_LINUX
    if (bar_index >= 6) return Result<std::vector<uint8_t>>({}, "bad BAR index");
    const auto& bar = dev.bars[bar_index];
    if (!bar.valid || offset + size > bar.size) {
        return Result<std::vector<uint8_t>>({}, "BAR read exceeds resource size");
    }
    std::string path =
        dev.sysfs_path + "/resource" + std::to_string(bar_index);
    int fd = ::open(path.c_str(), O_RDWR | O_SYNC);
    if (fd < 0) {
        return Result<std::vector<uint8_t>>({}, "Cannot open " + path);
    }
    long ps = ::sysconf(_SC_PAGESIZE);
    size_t page_size = ps > 0 ? static_cast<size_t>(ps) : 4096;
    size_t map_size = ((offset + size + page_size - 1) / page_size) * page_size;
    void* map =
        ::mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        ::close(fd);
        return Result<std::vector<uint8_t>>({}, "mmap of " + path + " failed");
    }
    std::vector<uint8_t> result(size);
    std::memcpy(result.data(), static_cast<uint8_t*>(map) + offset, size);
    ::munmap(map, map_size);
    ::close(fd);
    return result;
#else
    (void)dev;
    (void)bar_index;
    (void)offset;
    (void)size;
    return Result<std::vector<uint8_t>>({}, "bar_read requires Linux");
#endif
}

Result<void> bar_write(const PciDevice& dev, unsigned bar_index, uint64_t offset,
                       const std::vector<uint8_t>& data) noexcept {
#if LR_PLATFORM_LINUX
    if (bar_index >= 6) return Result<void>("bad BAR index");
    const auto& bar = dev.bars[bar_index];
    if (!bar.valid || offset + data.size() > bar.size) {
        return Result<void>("BAR write exceeds resource size");
    }
    std::string path =
        dev.sysfs_path + "/resource" + std::to_string(bar_index);
    int fd = ::open(path.c_str(), O_RDWR | O_SYNC);
    if (fd < 0) return Result<void>("Cannot open " + path);
    long ps = ::sysconf(_SC_PAGESIZE);
    size_t page_size = ps > 0 ? static_cast<size_t>(ps) : 4096;
    size_t map_size =
        ((offset + data.size() + page_size - 1) / page_size) * page_size;
    void* map =
        ::mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        ::close(fd);
        return Result<void>("mmap failed");
    }
    std::memcpy(static_cast<uint8_t*>(map) + offset, data.data(), data.size());
    ::munmap(map, map_size);
    ::close(fd);
    return Result<void>();
#else
    (void)dev;
    (void)bar_index;
    (void)offset;
    (void)data;
    return Result<void>("bar_write requires Linux");
#endif
}

Result<std::vector<uint8_t>> bar_read(const PciDevice& dev, uint64_t offset,
                                      size_t size) noexcept {
    return bar_read(dev, 0, offset, size);
}

Result<void> bar_write(const PciDevice& dev, uint64_t offset,
                       const std::vector<uint8_t>& data) noexcept {
    return bar_write(dev, 0, offset, data);
}

Result<std::vector<uint8_t>> config_read(const PciDevice& dev, uint64_t offset,
                                         size_t size) noexcept {
#if LR_PLATFORM_LINUX
    std::string conf_path = dev.sysfs_path + "/config";
    int fd = ::open(conf_path.c_str(), O_RDWR);
    if (fd < 0) {
        return Result<std::vector<uint8_t>>({}, "Cannot open " + conf_path);
    }
    std::vector<uint8_t> result(size);
    ssize_t bytes =
        ::pread(fd, result.data(), size, static_cast<off_t>(offset));
    ::close(fd);
    if (bytes < 0 || static_cast<size_t>(bytes) != size) {
        return Result<std::vector<uint8_t>>({}, "Short config read");
    }
    return result;
#else
    (void)dev;
    (void)offset;
    (void)size;
    return Result<std::vector<uint8_t>>({}, "config_read requires Linux");
#endif
}

Result<void> config_write(const PciDevice& dev, uint64_t offset,
                          const std::vector<uint8_t>& data) noexcept {
#if LR_PLATFORM_LINUX
    std::string conf_path = dev.sysfs_path + "/config";
    int fd = ::open(conf_path.c_str(), O_RDWR);
    if (fd < 0) return Result<void>("Cannot open " + conf_path);
    ssize_t bytes =
        ::pwrite(fd, data.data(), data.size(), static_cast<off_t>(offset));
    ::close(fd);
    if (bytes < 0 || static_cast<size_t>(bytes) != data.size()) {
        return Result<void>("Short config write");
    }
    return Result<void>();
#else
    (void)dev;
    (void)offset;
    (void)data;
    return Result<void>("config_write requires Linux");
#endif
}

Result<bool> ioport_accessible() noexcept {
#if LR_PLATFORM_LINUX && (defined(__i386__) || defined(__x86_64__))
    if (::iopl(3) == 0) {
        ::iopl(0);
        return true;
    }
    return Result<bool>(false, "iopl(3) failed (need root)");
#else
    return Result<bool>(false, "ioport not available");
#endif
}

Result<std::vector<uint8_t>> port_read(uint16_t port, size_t size) noexcept {
#if LR_PLATFORM_LINUX && (defined(__i386__) || defined(__x86_64__))
    std::vector<uint8_t> result(size);
    if (::iopl(3) < 0 && ::ioperm(port, static_cast<unsigned>(size), 1) < 0) {
        return Result<std::vector<uint8_t>>({}, "ioperm failed (need root)");
    }
    for (size_t i = 0; i < size;) {
        if (size - i >= 4) {
            uint32_t v = ::inl(static_cast<unsigned short>(port + i));
            std::memcpy(result.data() + i, &v, 4);
            i += 4;
        } else if (size - i >= 2) {
            uint16_t v = ::inw(static_cast<unsigned short>(port + i));
            std::memcpy(result.data() + i, &v, 2);
            i += 2;
        } else {
            result[i] = ::inb(static_cast<unsigned short>(port + i));
            ++i;
        }
    }
    return result;
#else
    (void)port;
    (void)size;
    return Result<std::vector<uint8_t>>({}, "port_read requires Linux x86");
#endif
}

Result<void> port_write(uint16_t port,
                        const std::vector<uint8_t>& data) noexcept {
#if LR_PLATFORM_LINUX && (defined(__i386__) || defined(__x86_64__))
    if (::iopl(3) < 0 &&
        ::ioperm(port, static_cast<unsigned>(data.size()), 1) < 0) {
        return Result<void>("ioperm failed (need root)");
    }
    for (size_t i = 0; i < data.size();) {
        if (data.size() - i >= 4) {
            uint32_t v;
            std::memcpy(&v, data.data() + i, 4);
            ::outl(v, static_cast<unsigned short>(port + i));
            i += 4;
        } else if (data.size() - i >= 2) {
            uint16_t v;
            std::memcpy(&v, data.data() + i, 2);
            ::outw(v, static_cast<unsigned short>(port + i));
            i += 2;
        } else {
            ::outb(data[i], static_cast<unsigned short>(port + i));
            ++i;
        }
    }
    return Result<void>();
#else
    (void)port;
    (void)data;
    return Result<void>("port_write requires Linux x86");
#endif
}

Result<std::string> iommu_group_for(const PciDevice& dev) noexcept {
    if (!dev.iommu_group.empty()) return dev.iommu_group;
#if LR_PLATFORM_LINUX
    std::string g = readlink_basename(dev.sysfs_path + "/iommu_group");
    if (g.empty()) return Result<std::string>({}, "no iommu_group");
    return g;
#else
    (void)dev;
    return Result<std::string>({}, "iommu_group_for requires Linux");
#endif
}

Result<bool> iommu_enabled() noexcept {
#if LR_PLATFORM_LINUX
    // Intel VT-d / AMD-Vi: presence of /sys/class/iommu or dmar
    struct stat st {};
    if (::stat("/sys/class/iommu", &st) == 0) return true;
    if (::stat("/sys/devices/virtual/iommu", &st) == 0) return true;
    std::ifstream cmdline("/proc/cmdline");
    std::string line;
    std::getline(cmdline, line);
    if (line.find("intel_iommu=on") != std::string::npos ||
        line.find("amd_iommu=on") != std::string::npos) {
        return true;
    }
    return Result<bool>(false, "IOMMU not detected");
#else
    return Result<bool>(false, "iommu_enabled requires Linux");
#endif
}

} // namespace real::linux::pcie
