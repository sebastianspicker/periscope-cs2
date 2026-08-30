// memory.hpp — Cross-platform memory mapping abstraction.
// Used by DMA backends, physical-memory readers, and SMM buffers.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace real {

/// Describes a mapped memory region.
struct MappedRegion {
  void* base = nullptr;
  std::size_t size = 0;
  std::uint64_t physical_address = 0;
};

/// Read physical memory via platform-specific interface.
/// On Windows: \\.\PhysicalMemory or driver MmMapIoSpace.
/// On Linux: /dev/mem or /dev/port.
Result<std::vector<std::uint8_t>> read_physical(std::uint64_t phys_addr,
                                                 std::size_t size);

/// Map physical memory into process address space.
Result<MappedRegion> map_physical(std::uint64_t phys_addr, std::size_t size);

/// Unmap a previously mapped physical region.
Result<void> unmap_physical(MappedRegion region);

/// Read virtual memory of a target process.
/// Uses OpenProcess/ReadProcessMemory on Windows, process_vm_readv on Linux.
Result<std::vector<std::uint8_t>> read_virtual(std::uint32_t pid,
                                                std::uint64_t addr,
                                                std::size_t size);

/// Read virtual memory without a handle (direct syscall path on Windows).
Result<std::vector<std::uint8_t>> read_virtual_direct(std::uint32_t pid,
                                                       std::uint64_t addr,
                                                       std::size_t size);

/// Get the physical address backing a virtual address.
Result<std::uint64_t> virtual_to_physical(std::uint32_t pid,
                                           std::uint64_t virt_addr);

/// Read memory from the current process using NtReadVirtualMemory on
/// NtCurrentProcess() instead of opening a handle to self.
/// Avoids creating an OpenProcess handle visible in the handle table.
Result<std::vector<std::uint8_t>> read_self_memory(std::uint64_t addr,
                                                    std::size_t size);

}  // namespace real
