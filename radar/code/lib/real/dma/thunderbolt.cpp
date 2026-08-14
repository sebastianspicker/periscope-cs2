// thunderbolt.cpp -- Thunderbolt and USB DFU checks for the educational DMA lab.

#include "real/dma/dma_backend.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include <setupapi.h>
#  pragma comment(lib, "setupapi.lib")
#elif LR_PLATFORM_LINUX
#  include <dirent.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

namespace real::dma {

#if LR_PLATFORM_LINUX
static Result<uint64_t> read_sysfs_hex(const std::string& path) {
  FILE* file = fopen(path.c_str(), "r");
  if (!file) return Result<uint64_t>(0, "Cannot open " + path);
  unsigned long long value = 0;
  const bool parsed = fscanf(file, "%llx", &value) == 1;
  fclose(file);
  return parsed ? Result<uint64_t>(static_cast<uint64_t>(value))
                : Result<uint64_t>(0, "Parse error in " + path);
}

static Result<std::string> read_sysfs_string(const std::string& path) {
  FILE* file = fopen(path.c_str(), "r");
  if (!file) return Result<std::string>({}, "Cannot open " + path);
  char buf[256] = {};
  if (!fgets(buf, sizeof(buf), file)) {
    fclose(file);
    return Result<std::string>({}, "Read error in " + path);
  }
  fclose(file);
  // trim newline
  size_t n = std::strlen(buf);
  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) buf[--n] = 0;
  return std::string(buf);
}
#endif

// ── Thunderbolt DMA ───────────────────────────────────────────────
Result<bool> thunderbolt_available() {
#if LR_PLATFORM_LINUX
  DIR* directory = opendir("/sys/bus/thunderbolt/devices/");
  if (!directory) {
    // Fallback: look for thunderbolt PCI drivers on the bus.
    auto devices = enum_pci_devices();
    if (devices) {
      for (const auto& d : *devices) {
        if (d.driver.find("thunderbolt") != std::string::npos ||
            d.driver.find("nhi") != std::string::npos) {
          return Result<bool>(true);
        }
        // Intel Thunderbolt / USB4 controllers
        if (d.vendor_id == 0x8086 &&
            (d.device_id == 0x15D2 || d.device_id == 0x15D3 ||
             d.device_id == 0x15EA || d.device_id == 0x15EB ||
             d.device_id == 0x1134 || d.device_id == 0x1137)) {
          return Result<bool>(true);
        }
      }
    }
    return Result<bool>(false, "No Thunderbolt devices");
  }
  int device_count = 0;
  bool security_none = false;
  while (dirent* entry = readdir(directory)) {
    if (entry->d_name[0] == '.') continue;
    ++device_count;
    std::string base =
        std::string("/sys/bus/thunderbolt/devices/") + entry->d_name;
    auto level = read_sysfs_string(base + "/security");
    if (level && (*level == "none" || *level == "user" || *level == "0")) {
      security_none = true;
    }
  }
  closedir(directory);
  if (device_count == 0) return Result<bool>(false, "No Thunderbolt connected");
  if (security_none) {
#ifndef NDEBUG
    std::printf("[dma] Thunderbolt security level permits DMA\n");
#endif
  }
  return Result<bool>(true);

#elif LR_PLATFORM_WINDOWS
  HKEY key = nullptr;
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "SYSTEM\\CurrentControlSet\\Services\\ThunderboltService", 0,
                    KEY_READ, &key) == ERROR_SUCCESS) {
    RegCloseKey(key);
    return Result<bool>(true);
  }
  if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                    "SYSTEM\\CurrentControlSet\\Services\\Thunderbolt", 0,
                    KEY_READ, &key) == ERROR_SUCCESS) {
    RegCloseKey(key);
    return Result<bool>(true);
  }
  // SetupAPI: look for Thunderbolt / USB4 class devices.
  HDEVINFO info =
      SetupDiGetClassDevsA(nullptr, "PCI", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
  if (info != INVALID_HANDLE_VALUE) {
    SP_DEVINFO_DATA data = {};
    data.cbSize = sizeof(data);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(info, i, &data); ++i) {
      char desc[256] = {};
      char hwid[512] = {};
      SetupDiGetDeviceRegistryPropertyA(info, &data, SPDRP_DEVICEDESC, nullptr,
                                        reinterpret_cast<PBYTE>(desc),
                                        sizeof(desc), nullptr);
      SetupDiGetDeviceRegistryPropertyA(info, &data, SPDRP_HARDWAREID, nullptr,
                                        reinterpret_cast<PBYTE>(hwid),
                                        sizeof(hwid), nullptr);
      if (std::strstr(desc, "Thunderbolt") || std::strstr(desc, "USB4") ||
          std::strstr(hwid, "VEN_8086&DEV_15D2") ||
          std::strstr(hwid, "VEN_8086&DEV_15EA") ||
          std::strstr(hwid, "VEN_8086&DEV_1134")) {
        SetupDiDestroyDeviceInfoList(info);
        return Result<bool>(true);
      }
    }
    SetupDiDestroyDeviceInfoList(info);
  }
  return Result<bool>(false, "Thunderbolt not found");
#else
  return Result<bool>(false, "Thunderbolt detection not supported");
#endif
}

Result<std::vector<uint8_t>> thunderbolt_dma_read(uint64_t phys_addr,
                                                  size_t size) {
  if (size == 0) return std::vector<uint8_t>{};

  auto available = thunderbolt_available();
  if (!available || !*available) {
    return Result<std::vector<uint8_t>>(
        {}, available ? available.error_msg.c_str()
                      : "Thunderbolt not available");
  }

  auto devices = enum_pci_devices();
  if (!devices) return Result<std::vector<uint8_t>>({}, devices.error_msg);

  for (const auto& device : *devices) {
    const bool is_tb =
        device.driver.find("thunderbolt") != std::string::npos ||
        device.driver.find("nhi") != std::string::npos ||
        device.driver.find("Thunderbolt") != std::string::npos ||
        (device.vendor_id == 0x8086 && device.bar0 != 0 &&
         (device.device_id == 0x15D2 || device.device_id == 0x15D3 ||
          device.device_id == 0x15EA || device.device_id == 0x15EB ||
          device.device_id == 0x1134 || device.device_id == 0x1137));
    if (!is_tb) continue;

    if (device.bar0 != 0 && device.bar0_size > 0) {
      // Prefer BAR-relative offset when the phys_addr sits inside the BAR;
      // otherwise attempt a host-memory DMA-style read via physmem after
      // programming would have completed (educational fallback).
      if (phys_addr >= device.bar0 &&
          phys_addr + size <= device.bar0 + device.bar0_size) {
        return pcie_bar_read(phys_addr, size, device);
      }
    }
  }

  // No programmable TB controller BAR path — fall back to physmem which is
  // what an unsecured Thunderbolt DMA master would ultimately expose.
  auto pm = physmem_read(phys_addr, size);
  if (pm) return pm;
  return Result<std::vector<uint8_t>>(
      {}, "Thunderbolt DMA path unavailable (no BAR / no physmem privilege)");
}

// ── USB DFU DMA ───────────────────────────────────────────────────
Result<bool> usb_dfu_dma_possible() {
#if LR_PLATFORM_LINUX
  DIR* directory = opendir("/sys/bus/usb/devices/");
  if (!directory) return Result<bool>(false, "Cannot open USB sysfs");
  bool found = false;
  while (dirent* entry = readdir(directory)) {
    if (entry->d_name[0] == '.') continue;
    const std::string base =
        std::string("/sys/bus/usb/devices/") + entry->d_name;
    const auto interface_class = read_sysfs_hex(base + "/bInterfaceClass");
    const auto device_class = read_sysfs_hex(base + "/bDeviceClass");
    // 0xFE = Application Specific; DFU uses subclass 0x01 at interface level.
    const auto iface_sub = read_sysfs_hex(base + "/bInterfaceSubClass");
    if ((interface_class && *interface_class == 0xFE &&
         (!iface_sub || *iface_sub == 0x01)) ||
        (device_class && *device_class == 0xFE)) {
      found = true;
      std::printf("[dma] USB DFU device found: %s\n", entry->d_name);
    }
  }
  closedir(directory);
  // Result(T,msg) always sets ok=false. Success true uses Result(true) alone.
  if (found) return Result<bool>(true);
  return Result<bool>(false, "No USB DFU devices");

#elif LR_PLATFORM_WINDOWS
  HDEVINFO dev_info = SetupDiGetClassDevsA(nullptr, "USB", nullptr,
                                           DIGCF_PRESENT | DIGCF_ALLCLASSES);
  if (dev_info == INVALID_HANDLE_VALUE)
    return Result<bool>(false, "SetupDiGetClassDevs failed");
  bool found = false;
  SP_DEVINFO_DATA dev_data = {};
  dev_data.cbSize = sizeof(SP_DEVINFO_DATA);
  for (DWORD i = 0; SetupDiEnumDeviceInfo(dev_info, i, &dev_data); ++i) {
    char hwid[512] = {};
    char devclass[64] = {};
    SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_data, SPDRP_HARDWAREID,
                                      nullptr, reinterpret_cast<PBYTE>(hwid),
                                      sizeof(hwid), nullptr);
    SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_data, SPDRP_CLASS, nullptr,
                                      reinterpret_cast<PBYTE>(devclass),
                                      sizeof(devclass), nullptr);
    // DFU class devices: Class FE or friendly class name.
    if (_stricmp(devclass, "DeviceFirmwareUpdate") == 0 ||
        _stricmp(devclass, "Firmware") == 0 ||
        std::strstr(hwid, "CLASS_FE") != nullptr ||
        std::strstr(hwid, "DFU") != nullptr) {
      found = true;
      std::printf("[dma] USB DFU device found via SetupAPI: %s\n", hwid);
    }
  }
  SetupDiDestroyDeviceInfoList(dev_info);
  if (found) return Result<bool>(true);
  return Result<bool>(false, "No USB DFU devices");
#else
  return Result<bool>(false, "USB DFU detection not supported");
#endif
}

}  // namespace real::dma
