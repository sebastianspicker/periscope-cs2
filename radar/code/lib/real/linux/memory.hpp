// memory.hpp — Linux physical/virtual memory access (SOTA research backend).
//
// Channels:
//   /dev/mem, /dev/crash     — physical memory
//   /proc/self|pid/pagemap   — VA → PA
//   process_vm_readv/writev  — cross-process virtual R/W
//   /proc/[pid]/mem         — procfs virtual R/W
//   ptrace PEEK/POKE         — debugger-style virtual R/W
//   /proc/kcore              — kernel virtual
//   /dev/aclab               — lab kernel module (T2)
//   finit_module/delete_module — kmod load/unload
//
// Every path documents SCAR / BLUE / MITIGATION in the .cpp.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"
#include "real/linux/pagemap.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace real::linux::mem {

// ── Physical memory ────────────────────────────────────────────────

Result<std::vector<uint8_t>> read_physical(uint64_t phys_addr,
                                           size_t size) noexcept;
Result<void> write_physical(uint64_t phys_addr,
                            const std::vector<uint8_t>& data) noexcept;
Result<std::vector<uint8_t>> read_physical_crash(uint64_t phys_addr,
                                                 size_t size) noexcept;
Result<std::vector<uint8_t>> read_physical_devmem(uint64_t phys_addr,
                                                  size_t size) noexcept;

// ── VA → PA translation ────────────────────────────────────────────

Result<uint64_t> virtual_to_physical(uint64_t virt_addr) noexcept;
Result<uint64_t> process_virtual_to_physical(uint32_t pid,
                                             uint64_t virt_addr) noexcept;
Result<pagemap::Entry> read_pagemap_entry(uint32_t pid,
                                          uint64_t virt_addr) noexcept;

// ── Process virtual memory ─────────────────────────────────────────

Result<std::vector<uint8_t>> read_process_memory(uint32_t pid, uint64_t addr,
                                                 size_t size) noexcept;
Result<void> write_process_memory(uint32_t pid, uint64_t addr,
                                  const std::vector<uint8_t>& data) noexcept;
Result<std::vector<uint8_t>> read_process_mem_procfs(uint32_t pid, uint64_t addr,
                                                     size_t size) noexcept;
Result<void> write_process_mem_procfs(uint32_t pid, uint64_t addr,
                                      const std::vector<uint8_t>& data) noexcept;
Result<std::vector<uint8_t>> read_process_memory_ptrace(uint32_t pid,
                                                        uint64_t addr,
                                                        size_t size) noexcept;
Result<void> write_process_memory_ptrace(
    uint32_t pid, uint64_t addr, const std::vector<uint8_t>& data) noexcept;

// ── Kernel virtual ─────────────────────────────────────────────────

Result<std::vector<uint8_t>> read_kernel_memory(uint64_t addr,
                                                size_t size) noexcept;

// ── Self memory (no foreign PID scar) ──────────────────────────────

Result<std::vector<uint8_t>> read_self_memory(uint64_t addr,
                                              size_t size) noexcept;

// ── Capability probes ──────────────────────────────────────────────

Result<bool> devmem_accessible() noexcept;
Result<bool> kmod_available() noexcept;
Result<bool> process_vm_available() noexcept;
Result<bool> ptrace_available() noexcept;
Result<bool> kcore_accessible() noexcept;

// ── Kernel module load/unload ──────────────────────────────────────

Result<void> load_kernel_module(const std::vector<uint8_t>& elf_data,
                                const std::string& params = "") noexcept;
Result<void> load_kernel_module_path(const std::string& path,
                                     const std::string& params = "") noexcept;
Result<void> unload_kernel_module(const std::string& module_name) noexcept;

// ── Page size helper (uses sysconf on Linux, 4096 fallback) ────────

uint64_t host_page_size() noexcept;

// ── Pure helpers always available (also used by unit tests) ────────

/// Bounds-check a physical read request before issuing syscalls.
inline bool phys_request_valid(uint64_t /*phys*/, size_t size) noexcept {
    return size > 0 && size <= (64ULL * 1024 * 1024); // hard cap 64 MiB
}

/// Bounds-check a virtual read request.
inline bool virt_request_valid(uint64_t /*addr*/, size_t size) noexcept {
    return size > 0 && size <= (64ULL * 1024 * 1024);
}

} // namespace real::linux::mem
