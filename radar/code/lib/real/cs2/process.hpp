// process.hpp — Real CS2 process attachment and querying.
//
// LESSON: Anti-cheats detect external readers by the handle graph.
// Opening cs2.exe with OpenProcess(PROCESS_VM_READ) creates a visible
// entry in the system handle table. This is the T0 detection vector.
// Real anti-cheats (VAC, VACnet) enumerate handles pointing to the
// game process. The only way to avoid this is to not have a handle
// (T2+: kernel, HV, DMA) — no amount of usermode renaming helps.
//
// Educational design:
//   REAL MODE:   Attaches to the real cs2.exe on the system.
//   SIM MODE:    Uses sim::World's process table.
//   Both teach the same handle-graph lesson.

#pragma once

#include "real/error.hpp"
#include "real/mode/mode.hpp"
#include "real/process.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::cs2 {

/// Result of attaching to the CS2 process.
struct AttachResult {
  bool attached = false;
  std::uint32_t pid = 0;
  std::uint64_t handle = 0;
  std::uint64_t base_address = 0;
  std::size_t image_size = 0;
  std::string error_msg;

  /// Human-readable summary for educational output.
  std::string describe() const;
};

/// Find the CS2 process by name.
/// Searches for "cs2.exe" (or configured name) in the process list.
/// Returns the first match. Errors if not found.
Result<real::ProcessInfo> find_cs2_process();

/// Open a handle to CS2 with specified access rights.
/// Default access: PROCESS_VM_READ | PROCESS_QUERY_INFORMATION.
/// Educational note: this handle is VISIBLE in the handle table.
// NOTE: This function opens CS2 with the specified access rights.
// The production radar path uses real::cs2::hijack::HijackReader
// which NEVER holds a VM_READ handle. This function is for
// educational/demo mode. When HIJACK_ONLY is defined, the
// default access is PROCESS_QUERY_LIMITED_INFORMATION only.
// Default: PROCESS_VM_READ | PROCESS_QUERY_INFORMATION (0x0410)
// Creates the classic T0 handle-table scar for educational demos.
Result<std::uint64_t> open_cs2_process(std::uint32_t pid,
                                        unsigned long access = 0x0410);

/// Get the base address of cs2.exe module.
/// Uses QueryFullProcessImageName + GetModuleInformation or
/// reads /proc/<pid>/maps to find the executable base.
Result<std::uint64_t> get_cs2_base_address(std::uint32_t pid,
                                            std::uint64_t process_handle);

/// Get the size of the cs2.exe image.
Result<std::size_t> get_cs2_image_size(std::uint32_t pid,
                                        std::uint64_t process_handle);

/// Try to attach to CS2 with retries.
/// Returns an AttachResult describing success or failure.
AttachResult attach_to_cs2(int retries = 3);

/// Detach from CS2 (close the handle).
void detach_from_cs2(std::uint64_t handle);

/// Check if the CS2 process is still running.
Result<bool> is_cs2_running(std::uint32_t pid);

/// Get all modules loaded in the CS2 process.
Result<std::vector<real::ProcessInfo::Module>> get_cs2_modules(
    std::uint32_t pid, std::uint64_t process_handle);

}  // namespace real::cs2
