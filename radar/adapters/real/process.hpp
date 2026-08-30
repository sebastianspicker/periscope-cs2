// process.hpp — Cross-platform process enumeration and handle operations.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real {

/// Information about a running process.
struct ProcessInfo {
  /// Basic information about a module loaded by the process.
  struct Module {
    std::string name;
    std::uint64_t base = 0;
    std::size_t size = 0;
  };

  std::uint32_t pid = 0;
  std::string name;
  std::string path;
  std::uint32_t parent_pid = 0;
};

/// Enumerate all running processes.
Result<std::vector<ProcessInfo>> enum_processes();

/// Find a process by name; returns first match or error.
Result<ProcessInfo> find_process(const std::string& name);

/// Open a process with specified access rights.
/// Returns an opaque handle value (HANDLE on Windows, pid on Linux).
Result<std::uint64_t> open_process(std::uint32_t pid, unsigned long access);

/// Close a previously opened process handle.
Result<void> close_process(std::uint64_t handle);

/// Get the parent PID of a process.
Result<std::uint32_t> get_parent_pid(std::uint32_t pid);

/// Get the process module path (full path to executable).
Result<std::string> get_process_path(std::uint32_t pid);

/// Check if a process is still running.
Result<bool> is_process_running(std::uint32_t pid);

}  // namespace real
