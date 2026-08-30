// pcie.cpp -- PCIe enumeration and BAR operations for the educational DMA lab.

#include "real/dma/dma_backend.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#  include "real/win/api_table.hpp"
#  include <setupapi.h>
#  include <cfgmgr32.h>
#  pragma comment(lib, "setupapi.lib")
#  pragma comment(lib, "cfgmgr32.lib")
#elif LR_PLATFORM_LINUX
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace real::dma {

#if LR_PLATFORM_LINUX
static Result<uint64_t> read_sysfs_hex(const std::string& path) {
  FILE* f = fopen(path.c_str(), "r");
  if (!f) return Result<uint64_t>(0, "Cannot open " + path);
  unsigned long long value = 0;
  if (fscanf(f, "%llx", &value) != 1) {
    fclose(f);
    return Result<uint64_t>(0, "Parse error in " + path);
  }
  fclose(f);
  return Result<uint64_t>(static_cast<uint64_t>(value));
}
#endif

// ── PCIe Device Enumeration ────────────────────────────────────────

Result<std::vector<PciDevice>> enum_pci_devices() {
  std::vector<PciDevice> devices;
#if LR_PLATFORM_LINUX
  DIR* dir = opendir("/sys/bus/pci/devices/");
  if (!dir)
    return Result<std::vector<PciDevice>>({}, "Cannot open /sys/bus/pci/devices");
  while (dirent* entry = readdir(dir)) {
    if (entry->d_name[0] == '.') continue;
    PciDevice dev;
    unsigned domain = 0, bus = 0, device = 0, function = 0;
    if (sscanf(entry->d_name, "%x:%x:%x.%x", &domain, &bus, &device, &function) !=
        4)
      continue;
    dev.address = {static_cast<uint8_t>(bus), static_cast<uint8_t>(device),
                   static_cast<uint8_t>(function)};
    const std::string base =
        std::string("/sys/bus/pci/devices/") + entry->d_name;
    auto vendor = read_sysfs_hex(base + "/vendor");
    auto device_id = read_sysfs_hex(base + "/device");
    if (vendor) dev.vendor_id = static_cast<uint16_t>(*vendor);
    if (device_id) dev.device_id = static_cast<uint16_t>(*device_id);

    auto class_code = read_sysfs_hex(base + "/class");
    if (class_code) {
      // sysfs class is 0xCCSSPP (class/subclass/prog-if)
      dev.class_code = static_cast<uint8_t>((*class_code >> 16) & 0xFF);
      dev.subclass = static_cast<uint8_t>((*class_code >> 8) & 0xFF);
    }

    struct stat st;
    const std::string driver_path = base + "/driver";
    if (lstat(driver_path.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
      char target[256] = {};
      const ssize_t length =
          readlink(driver_path.c_str(), target, sizeof(target) - 1);
      if (length > 0) {
        target[length] = '\0';
        const char* name = strrchr(target, '/');
        dev.driver = name ? name + 1 : target;
      }
    }

    FILE* resources = fopen((base + "/resource").c_str(), "r");
    if (resources) {
      char line[128];
      for (int bar = 0; bar < 6 && fgets(line, sizeof(line), resources); ++bar) {
        unsigned long long start = 0, end = 0, flags = 0;
        if (sscanf(line, "0x%llx 0x%llx 0x%llx", &start, &end, &flags) < 2)
          continue;
        const size_t length =
            end > start ? static_cast<size_t>(end - start + 1) : 0;
        if (bar == 0) {
          dev.bar0 = start;
          dev.bar0_size = length;
        }
        if (bar == 1) {
          dev.bar1 = start;
          dev.bar1_size = length;
        }
        // IORESOURCE_MEM | IORESOURCE_PREFETCH-ish — any non-zero memory BAR
        // marks DMA-relevant devices for lab enumeration.
        if (start > 0 && length > 0) dev.is_dma_capable = true;
      }
      fclose(resources);
    }
    devices.push_back(std::move(dev));
  }
  closedir(dir);

#elif LR_PLATFORM_WINDOWS
  HDEVINFO info =
      SetupDiGetClassDevsA(nullptr, "PCI", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
  if (info == INVALID_HANDLE_VALUE)
    return Result<std::vector<PciDevice>>({}, "SetupDiGetClassDevs failed");

  SP_DEVINFO_DATA data = {};
  data.cbSize = sizeof(data);
  for (DWORD index = 0; SetupDiEnumDeviceInfo(info, index, &data); ++index) {
    PciDevice dev;

    char hardware_id[512] = {};
    SetupDiGetDeviceRegistryPropertyA(
        info, &data, SPDRP_HARDWAREID, nullptr,
        reinterpret_cast<PBYTE>(hardware_id), sizeof(hardware_id), nullptr);
    unsigned vendor = 0, device = 0, subsys = 0, rev = 0;
    if (sscanf(hardware_id, "PCI\\VEN_%x&DEV_%x&SUBSYS_%x&REV_%x", &vendor,
               &device, &subsys, &rev) >= 2 ||
        sscanf(hardware_id, "PCI\\VEN_%x&DEV_%x", &vendor, &device) == 2) {
      dev.vendor_id = static_cast<uint16_t>(vendor);
      dev.device_id = static_cast<uint16_t>(device);
    }

    char driver[256] = {};
    if (SetupDiGetDeviceRegistryPropertyA(
            info, &data, SPDRP_DRIVER, nullptr,
            reinterpret_cast<PBYTE>(driver), sizeof(driver), nullptr)) {
      dev.driver = driver;
    }

    // Bus number
    DWORD bus_num = 0;
    if (SetupDiGetDeviceRegistryPropertyA(
            info, &data, SPDRP_BUSNUMBER, nullptr,
            reinterpret_cast<PBYTE>(&bus_num), sizeof(bus_num), nullptr)) {
      dev.address.bus = static_cast<uint8_t>(bus_num & 0xFF);
    }

    // Device/function packed in SPDRP_ADDRESS: ((device & 0x1F) << 16) | function
    DWORD address = 0;
    if (SetupDiGetDeviceRegistryPropertyA(
            info, &data, SPDRP_ADDRESS, nullptr,
            reinterpret_cast<PBYTE>(&address), sizeof(address), nullptr)) {
      dev.address.device = static_cast<uint8_t>((address >> 16) & 0x1F);
      dev.address.function = static_cast<uint8_t>(address & 0xFF);
    }

    // Class from SPDRP_CLASSGUID is not the PCI class code; try SPDRP_COMPATIBLEIDS
    // or the location path. Also pull memory resource descriptors for BARs.
    LOG_CONF log_conf = 0;
    CONFIGRET cr =
        CM_Get_First_Log_Conf(&log_conf, data.DevInst, ALLOC_LOG_CONF);
    if (cr != CR_SUCCESS) {
      cr = CM_Get_First_Log_Conf(&log_conf, data.DevInst, BOOT_LOG_CONF);
    }
    if (cr == CR_SUCCESS && log_conf) {
      RES_DES prev_rd = 0;
      RES_DES rd = 0;
      int mem_index = 0;
      while (CM_Get_Next_Res_Des(&rd, prev_rd ? prev_rd : log_conf,
                                 ResType_Mem, nullptr, 0) == CR_SUCCESS) {
        if (prev_rd) CM_Free_Res_Des_Handle(prev_rd);
        prev_rd = rd;

        ULONG rd_size = 0;
        if (CM_Get_Res_Des_Data_Size(&rd_size, rd, 0) != CR_SUCCESS ||
            rd_size < sizeof(MEM_RESOURCE))
          continue;
        std::vector<BYTE> buf(rd_size);
        if (CM_Get_Res_Des_Data(rd, buf.data(), rd_size, 0) != CR_SUCCESS)
          continue;
        auto* mem = reinterpret_cast<MEM_RESOURCE*>(buf.data());
        const uint64_t start =
            (static_cast<uint64_t>(mem->MEM_Header.MD_Alloc_Base));
        const uint64_t end =
            (static_cast<uint64_t>(mem->MEM_Header.MD_Alloc_End));
        const size_t length =
            end >= start ? static_cast<size_t>(end - start + 1) : 0;
        if (mem_index == 0) {
          dev.bar0 = start;
          dev.bar0_size = length;
        } else if (mem_index == 1) {
          dev.bar1 = start;
          dev.bar1_size = length;
        }
        if (start > 0 && length > 0) dev.is_dma_capable = true;
        ++mem_index;
      }
      if (prev_rd) CM_Free_Res_Des_Handle(prev_rd);
      CM_Free_Log_Conf_Handle(log_conf);
    }

    // Bridge class heuristic: service name contains "pci" + bridge, or
    // hardware ID class. Mark known DMA-related devices.
    char service[128] = {};
    if (SetupDiGetDeviceRegistryPropertyA(
            info, &data, SPDRP_SERVICE, nullptr,
            reinterpret_cast<PBYTE>(service), sizeof(service), nullptr)) {
      if (_stricmp(service, "pci") == 0 ||
          _stricmp(service, "pciide") == 0 ||
          strstr(service, "thunderbolt") != nullptr ||
          strstr(service, "Thunderbolt") != nullptr) {
        // Keep is_dma_capable from BAR presence; tag bridges.
        if (dev.bar0 == 0) {
          // Root complex / bridges may have no BAR but matter for ACS.
        }
      }
      if (dev.driver.empty()) dev.driver = service;
    }

    devices.push_back(std::move(dev));
  }
  SetupDiDestroyDeviceInfoList(info);
#else
  return Result<std::vector<PciDevice>>(
      {}, "PCI enumeration not supported on this platform");
#endif
  return devices;
}

Result<PciDevice> find_pci_device(uint16_t vendor_id, uint16_t device_id) {
  auto devices = enum_pci_devices();
  if (!devices) return Result<PciDevice>({}, devices.error_msg);
  for (const auto& device : *devices) {
    if (device.vendor_id == vendor_id && device.device_id == device_id)
      return device;
  }
  return Result<PciDevice>({}, "PCI device not found");
}

// ── PCIe BAR Read ──────────────────────────────────────────────────

struct BarMapping {
  int fd{-1};
#if LR_PLATFORM_WINDOWS
  HANDLE handle{INVALID_HANDLE_VALUE};
  void* view{nullptr};
#else
  void* map{nullptr};
#endif
  size_t size{0};
  uint64_t bar_base{0};
};

static std::mutex g_barMutex;
static std::map<uint64_t, BarMapping> g_barMappings;

static uint64_t bdf_key(const PciDevice& device) {
  return (static_cast<uint64_t>(device.address.bus) << 16) |
         (static_cast<uint64_t>(device.address.device) << 8) |
         device.address.function;
}

Result<std::vector<uint8_t>> pcie_bar_read(uint64_t phys_addr, size_t size,
                                           const PciDevice& device) {
#ifndef NDEBUG
  std::printf(
      "[dma] pcie_bar_read: phys=0x%llx size=%zu via %04x:%04x BAR0=0x%llx\n",
      static_cast<unsigned long long>(phys_addr), size, device.vendor_id,
      device.device_id, static_cast<unsigned long long>(device.bar0));
#endif

  if (size == 0) return std::vector<uint8_t>{};
  if (device.bar0 == 0 || device.bar0_size == 0) {
    return Result<std::vector<uint8_t>>({}, "PCIe BAR0 not available on device");
  }
  if (phys_addr < device.bar0 ||
      phys_addr - device.bar0 > device.bar0_size ||
      size > device.bar0_size - static_cast<size_t>(phys_addr - device.bar0)) {
    return Result<std::vector<uint8_t>>({}, "Requested range is outside PCIe BAR0");
  }

  const uint64_t key = bdf_key(device);
  const size_t offset = static_cast<size_t>(phys_addr - device.bar0);

#if LR_PLATFORM_LINUX
  std::lock_guard<std::mutex> lock(g_barMutex);
  auto it = g_barMappings.find(key);

  if (it == g_barMappings.end() || it->second.fd < 0) {
    char path[160];
    snprintf(path, sizeof(path),
             "/sys/bus/pci/devices/0000:%02x:%02x.%x/resource0",
             device.address.bus, device.address.device, device.address.function);
    int fd = open(path, O_RDWR | O_SYNC);
    if (fd < 0) {
      // Try without domain prefix variants already covered; try resource0 via
      // domain wildcard by scanning if needed — fall through to error.
      return Result<std::vector<uint8_t>>({}, "Cannot open PCIe resource0");
    }

    const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    const size_t map_size =
        ((device.bar0_size + page_size - 1) / page_size) * page_size;
    void* mapping =
        mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapping == MAP_FAILED) {
      close(fd);
      return Result<std::vector<uint8_t>>({}, "Cannot mmap PCIe BAR");
    }

    BarMapping bm;
    bm.fd = fd;
    bm.map = mapping;
    bm.size = map_size;
    bm.bar_base = device.bar0;
    g_barMappings[key] = std::move(bm);
    it = g_barMappings.find(key);
  }

  std::vector<uint8_t> result(size);
  std::memcpy(result.data(), static_cast<uint8_t*>(it->second.map) + offset,
              size);
  return result;

#elif LR_PLATFORM_WINDOWS
  // Windows: map the BAR physical range via the physmem path (section or
  // driver). Cache a single successful view when the kernel allows it.
  std::lock_guard<std::mutex> lock(g_barMutex);
  auto it = g_barMappings.find(key);

  if (it == g_barMappings.end() || it->second.view == nullptr) {
    // Prefer reading the BAR region through physmem_read into a transient
    // buffer when a persistent map is unavailable.
    // First try CreateFileMapping on PhysicalMemory for a reusable view.
    HANDLE phys = ::CreateFileA("\\\\.\\PhysicalMemory", GENERIC_READ,
                                FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0,
                                nullptr);
    if (phys != INVALID_HANDLE_VALUE) {
      // Direct ReadFile from the BAR base + offset.
      LARGE_INTEGER li;
      li.QuadPart = static_cast<LONGLONG>(phys_addr);
      DWORD got = 0;
      std::vector<uint8_t> result(size);
      if (SetFilePointerEx(phys, li, nullptr, FILE_BEGIN) &&
          ::ReadFile(phys, result.data(), static_cast<DWORD>(size), &got,
                     nullptr) &&
          got > 0) {
        result.resize(got);
        ::CloseHandle(phys);
        return result;
      }
      ::CloseHandle(phys);
    }

    // Fallback: one-shot physmem_read covering the requested range.
    auto once = physmem_read(phys_addr, size);
    if (once) return once;

    return Result<std::vector<uint8_t>>(
        {}, "PCIe BAR read failed: no physical-memory mapping available on Windows");
  }

  std::vector<uint8_t> result(size);
  std::memcpy(result.data(),
              static_cast<uint8_t*>(it->second.view) + offset, size);
  return result;

#else
  (void)offset;
  (void)key;
  return Result<std::vector<uint8_t>>(
      {}, "PCIe BAR read requires Linux or Windows with privileged access");
#endif
}

// ── PCIe Peer-to-Peer DMA ─────────────────────────────────────────

Result<std::vector<uint8_t>> pcie_peer_read(uint64_t phys_addr, size_t size,
                                            const PciDevice& reader,
                                            const PciDevice& writer) {
#ifndef NDEBUG
  std::printf("[dma] PCIe peer-to-peer DMA: %04x:%04x -> %04x:%04x\n",
              reader.vendor_id, reader.device_id, writer.vendor_id,
              writer.device_id);
  std::printf("[dma] Target PA: 0x%llx, size: %zu\n",
              (unsigned long long)phys_addr, size);
#endif

  if (reader.bar0 == 0 || writer.bar0 == 0) {
    return Result<std::vector<uint8_t>>(
        {}, "Both devices need valid BAR0 for P2P DMA");
  }
  if (size == 0) return std::vector<uint8_t>{};

  size_t xfer = size;
  if (xfer > writer.bar0_size) {
    xfer = writer.bar0_size;
#ifndef NDEBUG
    std::printf("[dma] Clamping read size to writer BAR0 size: %zu\n", xfer);
#endif
  }

#if LR_PLATFORM_LINUX
  // Educational P2P: program reader BAR if accessible, otherwise fall back to
  // physmem_read of the target and stage through writer BAR for observability.
  char reader_path[160];
  snprintf(reader_path, sizeof(reader_path),
           "/sys/bus/pci/devices/0000:%02x:%02x.%x/resource0",
           reader.address.bus, reader.address.device, reader.address.function);
  int reader_fd = open(reader_path, O_RDWR | O_SYNC);
  if (reader_fd < 0) {
    return Result<std::vector<uint8_t>>({}, "Cannot open reader BAR0");
  }

  char writer_path[160];
  snprintf(writer_path, sizeof(writer_path),
           "/sys/bus/pci/devices/0000:%02x:%02x.%x/resource0",
           writer.address.bus, writer.address.device, writer.address.function);
  int writer_fd = open(writer_path, O_RDWR | O_SYNC);
  if (writer_fd < 0) {
    close(reader_fd);
    return Result<std::vector<uint8_t>>({}, "Cannot open writer BAR0");
  }

  auto target_data = physmem_read(phys_addr, xfer);
  if (!target_data) {
    close(reader_fd);
    close(writer_fd);
    return Result<std::vector<uint8_t>>(
        {},
        std::string("Failed to read target physical memory: ") +
            target_data.error_msg.c_str());
  }

  size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  size_t writer_mmap_size = ((xfer + page_size - 1) / page_size) * page_size;
  void* writer_map = mmap(nullptr, writer_mmap_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, writer_fd, 0);
  if (writer_map == MAP_FAILED) {
    close(reader_fd);
    close(writer_fd);
    return Result<std::vector<uint8_t>>({}, "Cannot mmap writer BAR0");
  }

  // Stage data into writer BAR (simulates P2P DMA completion buffer).
  std::memcpy(writer_map, target_data->data(), xfer);

  std::vector<uint8_t> result(xfer);
  std::memcpy(result.data(), writer_map, xfer);

  // Optionally poke reader BAR control region (descriptor-style doorbell).
  size_t reader_mmap_size =
      ((reader.bar0_size + page_size - 1) / page_size) * page_size;
  if (reader_mmap_size == 0) reader_mmap_size = page_size;
  void* reader_map = mmap(nullptr, reader_mmap_size, PROT_READ | PROT_WRITE,
                          MAP_SHARED, reader_fd, 0);
  if (reader_map != MAP_FAILED) {
    // Write a minimal "P2P complete" marker at offset 0 if the BAR is large
    // enough — educational scar for AC detectors.
    if (reader.bar0_size >= 4) {
      volatile uint32_t* reg = static_cast<volatile uint32_t*>(reader_map);
      *reg = 0xD2A00001u;
    }
    munmap(reader_map, reader_mmap_size);
  }

  munmap(writer_map, writer_mmap_size);
  close(writer_fd);
  close(reader_fd);
  return result;

#elif LR_PLATFORM_WINDOWS
  // Windows P2P without a vendor DMA engine: stage via physmem + writer BAR
  // physical range readback.
  auto target_data = physmem_read(phys_addr, xfer);
  if (!target_data) {
    return Result<std::vector<uint8_t>>(
        {},
        std::string("P2P phys read failed: ") + target_data.error_msg.c_str());
  }

  // Best-effort write into writer BAR via physmem path is not generally
  // available; return the staged buffer as the educational P2P result and
  // also attempt BAR read of writer if mapped.
  auto bar_probe = pcie_bar_read(writer.bar0, xfer > 16 ? 16 : xfer, writer);
  (void)bar_probe;
  (void)reader;
  return std::move(*target_data);

#else
  (void)phys_addr;
  (void)size;
  (void)reader;
  (void)writer;
  return Result<std::vector<uint8_t>>(
      {}, "PCIe peer DMA requires Linux or Windows with root/admin access");
#endif
}

}  // namespace real::dma
