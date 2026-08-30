// acpi.cpp — ACPI table operations backend (RSDP, table read, DSDT override).
//
// TECHNIQUE: RSDP → RSDT/XSDT walk; OS firmware table APIs; optional
// physical overwrite of DSDT for firmware-persistence research.
//
// SCAR: Reading tables is normal; modifying DSDT invalidates checksums
// and TPM measurements of firmware/config.
//
// BLUE: Hash DSDT/SSDT against known-good; Secure Boot / measured boot.
//
// MITIGATION: UEFI Secure Boot, HVCI ACPI protection, locked tables.

#include "real/smm/smm_interface.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if LR_PLATFORM_LINUX
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <unistd.h>
#elif LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#endif

namespace real::smm {
namespace {

std::vector<std::uint8_t> read_file(const std::string& path) {
  FILE* file = std::fopen(path.c_str(), "rb");
  if (!file) return {};
  std::fseek(file, 0, SEEK_END);
  const long size = std::ftell(file);
  std::rewind(file);
  if (size <= 0) {
    std::fclose(file);
    return {};
  }
  std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
  std::fread(data.data(), 1, data.size(), file);
  std::fclose(file);
  return data;
}

#if LR_PLATFORM_LINUX
bool read_at(int fd, std::uint64_t address, void* buffer, std::size_t size) {
  return lseek(fd, static_cast<off_t>(address), SEEK_SET) >= 0 &&
         read(fd, buffer, size) == static_cast<ssize_t>(size);
}
#endif

}  // namespace

// ── find_rsdp ──────────────────────────────────────────────────────

Result<std::uint64_t> find_rsdp() {
#if LR_PLATFORM_LINUX
  auto data = read_file("/sys/firmware/efi/systab");
  if (!data.empty()) {
    std::string text(reinterpret_cast<char*>(data.data()), data.size());
    const auto pos = text.find("RSDP=");
    if (pos != std::string::npos) {
      std::uint64_t address = 0;
      std::sscanf(text.c_str() + static_cast<std::ptrdiff_t>(pos), "RSDP=0x%llx",
                  reinterpret_cast<unsigned long long*>(&address));
      if (address) {
        std::printf("[smm] RSDP from EFI systab: 0x%llx\n",
                    static_cast<unsigned long long>(address));
        return address;
      }
    }
  }

  int fd = open("/dev/mem", O_RDONLY | O_SYNC);
  if (fd < 0) {
    auto err = os_error("open /dev/mem for RSDP scan (need root)");
    return Result<std::uint64_t>(0, err.error_msg);
  }
  for (std::uint64_t offset = 0xE0000; offset < 0x100000; offset += 16) {
    std::uint8_t signature[36]{};
    if (read_at(fd, offset, signature, 20) && rsdp_valid(signature, 20)) {
      close(fd);
      std::printf("[smm] RSDP found in BIOS memory: 0x%llx\n",
                  static_cast<unsigned long long>(offset));
      return offset;
    }
  }
  close(fd);
  return Result<std::uint64_t>(0, "RSDP not found in EFI or BIOS memory");
#elif LR_PLATFORM_WINDOWS
  // GetSystemFirmwareTable does not return the RSDP physical address directly.
  // Enumerate ACPI tables as proof of firmware ACPI stack presence; RSDP PA
  // still needs physical memory for a true walk.
  DWORD size = GetSystemFirmwareTable('ACPI', 'PCAF', nullptr, 0);  // FACP LE
  if (!size) size = GetSystemFirmwareTable('RSDT', 0, nullptr, 0);
  if (size) {
    // Synthetic non-zero token: physical RSDP unknown from usermode API.
    // Callers that need PA should use lab physmem; we report 0 with detail.
    return Result<std::uint64_t>(
        0, "RSDP physical address not exposed by GetSystemFirmwareTable");
  }
  return Result<std::uint64_t>(0, "RSDP not found via firmware table API");
#else
  return Result<std::uint64_t>(0, "RSDP lookup requires UEFI firmware");
#endif
}

// ── read_acpi_table ────────────────────────────────────────────────

Result<std::vector<std::uint8_t>> read_acpi_table(const char* signature) {
  if (!signature || std::strlen(signature) != 4) {
    return Result<std::vector<std::uint8_t>>(
        {}, "Invalid ACPI signature (need 4 chars)");
  }
  std::printf("[smm] read_acpi_table: %s\n", signature);

#if LR_PLATFORM_LINUX
  char sysfs_path[256];
  std::snprintf(sysfs_path, sizeof(sysfs_path),
                "/sys/firmware/acpi/tables/%.4s", signature);
  auto table = read_file(sysfs_path);
  if (!table.empty()) {
    if (!acpi_checksum_valid(table.data(), table.size())) {
      std::printf("[smm] WARNING: ACPI %.4s checksum invalid\n", signature);
    }
    return table;
  }

  // SSDT and multi-instance tables live under /sys/firmware/acpi/tables/data
  // or as SSDT1, SSDT2 — try data path for exact name.
  std::snprintf(sysfs_path, sizeof(sysfs_path),
                "/sys/firmware/acpi/tables/data/%.4s", signature);
  table = read_file(sysfs_path);
  if (!table.empty()) return table;

  auto rsdp_address = find_rsdp();
  if (!rsdp_address) {
    return Result<std::vector<std::uint8_t>>({}, rsdp_address.error_msg);
  }
  int fd = open("/dev/mem", O_RDONLY | O_SYNC);
  if (fd < 0) {
    auto err = os_error("open /dev/mem failed");
    return Result<std::vector<std::uint8_t>>({}, err.error_msg);
  }

  std::uint8_t rsdp[36]{};
  if (!read_at(fd, *rsdp_address, rsdp, sizeof(rsdp))) {
    close(fd);
    return Result<std::vector<std::uint8_t>>({}, "failed to read RSDP");
  }
  auto addrs = parse_rsdp_addresses(rsdp, sizeof(rsdp));
  if (!addrs.ok) {
    close(fd);
    return Result<std::vector<std::uint8_t>>({}, "invalid RSDP");
  }
  const bool is_xsdt = addrs.revision >= 2 && addrs.xsdt != 0;
  const std::uint64_t root_address = is_xsdt ? addrs.xsdt : addrs.rsdt;
  if (!root_address) {
    close(fd);
    return Result<std::vector<std::uint8_t>>({}, "No RSDT/XSDT");
  }

  std::uint8_t root_header[36]{};
  if (!read_at(fd, root_address, root_header, sizeof(root_header))) {
    close(fd);
    return Result<std::vector<std::uint8_t>>({}, "failed to read RSDT/XSDT header");
  }
  std::uint32_t root_length = 0;
  std::memcpy(&root_length, root_header + 4, sizeof(root_length));
  const std::size_t entry_size = is_xsdt ? sizeof(std::uint64_t) : sizeof(std::uint32_t);
  if (root_length < sizeof(root_header)) {
    close(fd);
    return Result<std::vector<std::uint8_t>>({}, "invalid RSDT/XSDT length");
  }

  const std::size_t entry_count =
      (root_length - sizeof(root_header)) / entry_size;
  for (std::size_t i = 0; i < entry_count; ++i) {
    std::uint64_t entry_address = 0;
    if (is_xsdt) {
      if (!read_at(fd, root_address + sizeof(root_header) + i * entry_size,
                   &entry_address, sizeof(entry_address)))
        continue;
    } else {
      std::uint32_t entry_address32 = 0;
      if (!read_at(fd, root_address + sizeof(root_header) + i * entry_size,
                   &entry_address32, sizeof(entry_address32)))
        continue;
      entry_address = entry_address32;
    }
    std::uint8_t header[8]{};
    if (!read_at(fd, entry_address, header, sizeof(header)) ||
        std::memcmp(header, signature, 4) != 0)
      continue;
    std::uint32_t table_length = 0;
    std::memcpy(&table_length, header + 4, sizeof(table_length));
    if (table_length < 36) continue;
    std::vector<std::uint8_t> found(table_length);
    if (!read_at(fd, entry_address, found.data(), found.size())) continue;
    close(fd);
    std::printf("[smm] ACPI table %.4s: %u bytes\n", signature, table_length);
    return found;
  }
  close(fd);
  return Result<std::vector<std::uint8_t>>(
      {}, "ACPI table " + std::string(signature) + " not found");

#elif LR_PLATFORM_WINDOWS
  const std::uint32_t fourcc =
      static_cast<std::uint32_t>(static_cast<std::uint8_t>(signature[0])) |
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(signature[1])) << 8) |
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(signature[2])) << 16) |
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(signature[3])) << 24);
  DWORD size = GetSystemFirmwareTable('ACPI', fourcc, nullptr, 0);
  if (!size) {
    return Result<std::vector<std::uint8_t>>({}, "ACPI table not found");
  }
  std::vector<std::uint8_t> table(size);
  if (!GetSystemFirmwareTable('ACPI', fourcc, table.data(), size)) {
    auto error = os_error("GetSystemFirmwareTable");
    return Result<std::vector<std::uint8_t>>({}, error.error_msg);
  }
  return table;
#else
  (void)signature;
  return Result<std::vector<std::uint8_t>>(
      {}, "ACPI table read requires Linux or Windows");
#endif
}

Result<std::vector<std::uint8_t>> read_acpi_dsdt() {
  return read_acpi_table("DSDT");
}

Result<FadtSmiInfo> read_fadt_smi_info() {
  auto facp = read_acpi_table("FACP");
  if (!facp) return Result<FadtSmiInfo>({}, facp.error_msg);
  auto info = parse_fadt_smi(facp->data(), facp->size());
  if (!info.ok) {
    return Result<FadtSmiInfo>(info, "FADT SMI_CMD not present");
  }
  std::printf("[smm] FADT SMI_CMD=0x%x enable=0x%02x disable=0x%02x\n",
              info.smi_cmd, info.acpi_enable, info.acpi_disable);
  return info;
}

Result<std::vector<std::string>> list_acpi_tables() {
  std::vector<std::string> names;
#if LR_PLATFORM_LINUX
  DIR* dir = opendir("/sys/firmware/acpi/tables");
  if (!dir) {
    auto err = os_error("opendir /sys/firmware/acpi/tables");
    return Result<std::vector<std::string>>({}, err.error_msg);
  }
  while (dirent* ent = readdir(dir)) {
    if (ent->d_name[0] == '.') continue;
    if (std::strcmp(ent->d_name, "data") == 0) continue;
    names.emplace_back(ent->d_name);
  }
  closedir(dir);
  return names;
#elif LR_PLATFORM_WINDOWS
  // Probe common signatures via GetSystemFirmwareTable.
  static const char* kCommon[] = {"FACP", "DSDT", "SSDT", "APIC", "MCFG",
                                  "HPET", "BGRT", "TPM2", "DMAR", "WAET",
                                  "SLIT", "SRAT", "FACS", "BERT", "HEST"};
  for (const char* sig : kCommon) {
    auto t = read_acpi_table(sig);
    if (t) names.emplace_back(sig);
  }
  return names;
#else
  return Result<std::vector<std::string>>({}, "ACPI list requires Linux or Windows");
#endif
}

// ── override_acpi_dsdt ─────────────────────────────────────────────

Result<void> override_acpi_dsdt(const std::vector<std::uint8_t>& new_dsdt) {
#if LR_PLATFORM_LINUX
  std::printf("[smm] override_acpi_dsdt: %zu bytes\n", new_dsdt.size());
  if (new_dsdt.size() < 36) {
    return Result<void>("DSDT too small");
  }

  auto current_dsdt = read_acpi_dsdt();
  if (!current_dsdt) {
    return Result<void>(std::string("Cannot find current DSDT: ") +
                        current_dsdt.error_msg.c_str());
  }

  std::uint64_t dsdt_phys_addr = 0;
  int fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd < 0) {
    return Result<void>(
        "DSDT override requires root and CONFIG_STRICT_DEVMEM disabled");
  }

  auto rsdp_result = find_rsdp();
  if (!rsdp_result) {
    close(fd);
    return Result<void>(std::string("Cannot find RSDP: ") +
                        rsdp_result.error_msg.c_str());
  }

  std::uint64_t rsdp = *rsdp_result;
  std::uint8_t rsdp_buf[64] = {};
  pread(fd, rsdp_buf, 64, static_cast<off_t>(rsdp));
  auto addrs = parse_rsdp_addresses(rsdp_buf, 36);
  if (!addrs.ok) {
    close(fd);
    return Result<void>("Invalid RSDP while locating DSDT");
  }

  // DSDT is referenced from FADT, not always as a direct RSDT entry.
  // Walk FADT first (X_DSDT at offset 140 for ACPI 2.0+, DSDT at 40).
  auto facp = read_acpi_table("FACP");
  if (facp && facp->size() >= 44) {
    std::uint32_t dsdt32 = 0;
    std::memcpy(&dsdt32, facp->data() + 40, 4);
    dsdt_phys_addr = dsdt32;
    if (facp->size() >= 148) {
      std::uint64_t xdsdt = 0;
      std::memcpy(&xdsdt, facp->data() + 140, 8);
      if (xdsdt) dsdt_phys_addr = xdsdt;
    }
  }

  if (!dsdt_phys_addr) {
    // Fallback: scan root table entries for "DSDT" signature.
    const bool is_xsdt = addrs.revision >= 2 && addrs.xsdt != 0;
    const std::uint64_t rsdt_addr = is_xsdt ? addrs.xsdt : addrs.rsdt;
    std::uint8_t rsdt_header[36] = {};
    pread(fd, rsdt_header, 36, static_cast<off_t>(rsdt_addr));
    std::uint32_t length = 0;
    std::memcpy(&length, rsdt_header + 4, 4);
    const int entry_size = is_xsdt ? 8 : 4;
    const int entry_count =
        length > 36 ? static_cast<int>((length - 36) / entry_size) : 0;
    for (int i = 0; i < entry_count; ++i) {
      std::uint64_t entry_addr = 0;
      if (is_xsdt) {
        pread(fd, &entry_addr, 8,
              static_cast<off_t>(rsdt_addr + 36 + static_cast<std::uint64_t>(i) * 8));
      } else {
        std::uint32_t entry32 = 0;
        pread(fd, &entry32, 4,
              static_cast<off_t>(rsdt_addr + 36 + static_cast<std::uint64_t>(i) * 4));
        entry_addr = entry32;
      }
      char sig[5] = {};
      pread(fd, sig, 4, static_cast<off_t>(entry_addr));
      if (std::memcmp(sig, "DSDT", 4) == 0) {
        dsdt_phys_addr = entry_addr;
        break;
      }
    }
  }

  if (!dsdt_phys_addr) {
    close(fd);
    return Result<void>("DSDT table not found in ACPI tables");
  }

  std::uint8_t dsdt_header[36] = {};
  pread(fd, dsdt_header, 36, static_cast<off_t>(dsdt_phys_addr));
  std::uint32_t dsdt_current_size = 0;
  std::memcpy(&dsdt_current_size, dsdt_header + 4, 4);

  std::printf("[smm] DSDT found at PA 0x%llx, current size=%u, new size=%zu\n",
              static_cast<unsigned long long>(dsdt_phys_addr), dsdt_current_size,
              new_dsdt.size());

  if (new_dsdt.size() > dsdt_current_size) {
    close(fd);
    return Result<void>("New DSDT larger than current; in-place override unsafe");
  }

  constexpr std::size_t page_size = 4096;
  const std::uint64_t page_start = dsdt_phys_addr & ~(static_cast<std::uint64_t>(page_size - 1));
  const std::size_t write_size = new_dsdt.size();
  const std::size_t map_size =
      ((dsdt_phys_addr - page_start + write_size + page_size - 1) / page_size) *
      page_size;

  void* map = mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                   static_cast<off_t>(page_start));
  if (map == MAP_FAILED) {
    close(fd);
    return Result<void>("Cannot mmap DSDT region for write");
  }

  std::vector<std::uint8_t> write_data = new_dsdt;
  acpi_fix_checksum(write_data, 9);

  const std::size_t offset_in_page =
      static_cast<std::size_t>(dsdt_phys_addr - page_start);
  std::memcpy(static_cast<std::uint8_t*>(map) + offset_in_page, write_data.data(),
              write_data.size());
  munmap(map, map_size);
  close(fd);

  std::printf("[smm] DSDT overwritten at PA 0x%llx (%zu bytes)\n",
              static_cast<unsigned long long>(dsdt_phys_addr), write_data.size());
  std::printf("[smm] NOTE: Reboot / AML reparse required for full effect\n");
  return Result<void>();
#else
  (void)new_dsdt;
  std::printf(
      "[smm] override_acpi_dsdt: requires Linux /dev/mem or Windows T2 driver\n");
  return Result<void>(
      "DSDT override requires /dev/mem (Linux) or kernel driver");
#endif
}

}  // namespace real::smm
