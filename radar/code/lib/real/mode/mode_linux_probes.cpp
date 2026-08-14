// mode_linux_probes.cpp — Linux device/sysfs tier probes.

#include "real/mode/mode_internal.hpp"

#if LR_PLATFORM_LINUX
// OBF is Windows-oriented; on Linux use identity for non-sensitive labels.
#ifndef OBF
#define OBF(s) (s)
#endif
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <string>

namespace real::mode::detail {

using real::mode::detail::contains_ci;

bool linux_open_ro(const char* path) {
  int fd = ::open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;
  ::close(fd);
  return true;
}

bool linux_path_exists(const char* path) {
  struct stat st{};
  return ::stat(path, &st) == 0;
}

bool linux_read_first_line(const char* path, std::string& out) {
  std::ifstream in(path);
  if (!in) return false;
  if (!std::getline(in, out)) return false;
  while (!out.empty() && (out.back() == '\r' || out.back() == '\n')) {
    out.pop_back();
  }
  return true;
}

bool linux_pci_dma_signals(std::string& reason) {
  DIR* pci_dir = ::opendir("/sys/bus/pci/devices/");
  if (!pci_dir) {
    reason = "/sys/bus/pci/devices unavailable";
    return false;
  }

  bool found = false;
  std::string detail;
  while (dirent* entry = ::readdir(pci_dir)) {
    if (!entry->d_name || entry->d_name[0] == '.') continue;
    const std::string base =
        std::string("/sys/bus/pci/devices/") + entry->d_name;

    std::string cls;
    if (linux_read_first_line((base + "/class").c_str(), cls)) {
      // DMA controller class 0x058000; PCI bridge 0x060400 often Thunderbolt.
      if (cls.find("058000") != std::string::npos) {
        found = true;
        detail = std::string("PCI DMA controller class at ") + entry->d_name;
        break;
      }
      // Thunderbolt host often appears as PCI bridge with thunderbolt driver.
      if (cls.find("060400") != std::string::npos) {
        std::string driver;
        char link[256] = {};
        const std::string dlink = base + "/driver";
        const ssize_t n = ::readlink(dlink.c_str(), link, sizeof(link) - 1);
        if (n > 0) {
          link[n] = 0;
          if (contains_ci(link, "thunderbolt") || contains_ci(link, "nhi")) {
            found = true;
            detail = std::string("Thunderbolt PCI bridge at ") + entry->d_name;
            break;
          }
        }
      }
    }

    std::string vendor;
    if (linux_read_first_line((base + "/vendor").c_str(), vendor)) {
      // Normalize 0x10ee / 0x10EE
      auto is_vendor = [&](const char* hex) {
        return contains_ci(vendor.c_str(), hex);
      };
      if (is_vendor("10ee") || is_vendor("1172") || is_vendor("1204") ||
          is_vendor("1d50") || is_vendor("0403")) {
        found = true;
        detail = std::string("FPGA/DMA vendor PCI device at ") + entry->d_name +
                 " vendor=" + vendor;
        break;
      }
      // Intel Thunderbolt device IDs
      std::string device;
      if (linux_read_first_line((base + "/device").c_str(), device)) {
        if (is_vendor("8086") &&
            (contains_ci(device.c_str(), "15d2") ||
             contains_ci(device.c_str(), "15d3") ||
             contains_ci(device.c_str(), "15ea") ||
             contains_ci(device.c_str(), "15eb") ||
             contains_ci(device.c_str(), "1134") ||
             contains_ci(device.c_str(), "1137") ||
             contains_ci(device.c_str(), "a0ec") ||
             contains_ci(device.c_str(), "9a1b") ||
             contains_ci(device.c_str(), "466d"))) {
          found = true;
          detail = std::string("Intel Thunderbolt/USB4 controller at ") +
                   entry->d_name;
          break;
        }
      }
    }
  }
  ::closedir(pci_dir);

  if (found) {
    reason = detail;
    return true;
  }

  if (linux_path_exists("/sys/bus/thunderbolt/devices")) {
    DIR* tb = ::opendir("/sys/bus/thunderbolt/devices");
    if (tb) {
      int count = 0;
      while (dirent* e = ::readdir(tb)) {
        if (e->d_name && e->d_name[0] != '.') ++count;
      }
      ::closedir(tb);
      if (count > 0) {
        reason = "thunderbolt sysfs bus has devices";
        return true;
      }
    }
  }

  // Named research DMA device nodes
  static const char* const kDevs[] = {
      "/dev/fpga0", "/dev/pcileech", "/dev/ftdi", "/dev/xdma0"};
  for (const char* p : kDevs) {
    if (linux_path_exists(p)) {
      reason = std::string("DMA device node present: ") + p;
      return true;
    }
  }

  reason = "no Thunderbolt/USB4/FPGA/DMA-class device signals";
  return false;
}

}  // namespace real::mode::detail
#endif  // LR_PLATFORM_LINUX
