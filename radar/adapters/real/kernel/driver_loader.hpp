// driver_loader.hpp — SCM driver load/unload and kernel-module inventory.
//
// LESSON: A driver service and loaded module leave durable SCM, registry, disk,
// object-manager, and audit artifacts. Blue anti-cheats scan all of these.
// This backend implements the real SCM / module-enum paths used in T2 labs.

#pragma once

#include "real/error.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace real::kernel {

struct DriverLoadResult {
  bool loaded = false;
  std::string driver_name;
  std::string service_name;
  std::string image_path;
  std::uint64_t error_code = 0;
  std::string detail;

  std::string describe() const;
};

/// Load a kernel driver via the Service Control Manager (CreateService + StartService).
/// On non-Windows platforms returns an explicit typed error (never silent success).
Result<DriverLoadResult> load_driver(const std::string& driver_path,
                                     const std::string& service_name,
                                     const std::string& display_name);

/// Stop and delete an SCM kernel-driver service.
Result<void> unload_driver(const std::string& service_name);

/// Check whether an SCM driver service exists without changing it.
Result<bool> driver_service_exists(const std::string& service_name);

struct KernelModuleInfo {
  std::string name;
  std::uint64_t base = 0;
  std::size_t size = 0;
  std::string path;
  bool is_signed = false;  // Signature verification is intentionally not implied.
};

/// Enumerate loaded kernel modules (Windows: SystemModuleInformation / PSAPI;
/// Linux: /proc/modules).
Result<std::vector<KernelModuleInfo>> enum_kernel_modules();
Result<bool> is_driver_loaded(const std::string& driver_name);
Result<std::uint64_t> get_driver_base(const std::string& driver_name);

// ── Pure helpers (unit-testable without SCM privileges) ────────────

/// Case-insensitive substring match used by is_driver_loaded / get_driver_base.
bool driver_name_matches(const std::string& module_name, const std::string& needle);

/// Strip directory and normalize a module basename for catalog comparison.
std::string driver_basename(const std::string& path_or_name);

}  // namespace real::kernel
