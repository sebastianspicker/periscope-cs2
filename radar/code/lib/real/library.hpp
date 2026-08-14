// library.hpp — Cross-platform dynamic library loading abstraction.
// Used for runtime ntdll resolution, driver loading, GPU API loading, etc.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real {

/// Opaque handle to a loaded dynamic library.
struct LibHandle {
  void* native = nullptr;  // HMODULE on Windows, void* on Linux
};

/// Load a dynamic library by path.
Result<LibHandle> load_library(const std::string& path);

/// Load a system library by name (searches standard paths).
Result<LibHandle> load_system_library(const std::string& name);

/// Resolve a symbol address in a loaded library.
Result<void*> get_symbol(LibHandle lib, const std::string& name);

/// Unload a previously loaded library.
Result<void> unload_library(LibHandle lib);

/// Get the base address of a loaded module in the current process.
Result<std::uint64_t> get_module_base(const std::string& name);

/// Get the size of a loaded module.
Result<std::size_t> get_module_size(const std::string& name);

/// Read from a loaded module's memory.
Result<std::vector<std::uint8_t>> read_module_memory(const std::string& name,
                                                      std::uint64_t offset,
                                                      std::size_t size);

}  // namespace real
