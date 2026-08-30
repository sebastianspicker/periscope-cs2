// syscall_ops.hpp — High-level process memory ops via indirect syscalls.
//
// All NT calls go through syscall_4/5/6/7 (indirect gadget in ntdll).
// No ntdll export stubs are executed for the sensitive path.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64

#include "real/win/windows_h.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace real::win {

/// Open a process via indirect NtOpenProcess. access is NT ACCESS_MASK.
Result<HANDLE> syscall_open_process(std::uint32_t pid, ACCESS_MASK access) noexcept;

/// Close a handle via indirect NtClose.
NTSTATUS syscall_close(HANDLE h) noexcept;

/// Read virtual memory via indirect NtReadVirtualMemory.
Result<std::size_t> syscall_read_memory(HANDLE process, std::uint64_t address,
                                        void* buffer, std::size_t size) noexcept;

/// Open + read + close convenience (still leaves a brief handle scar).
Result<std::vector<std::uint8_t>> syscall_read_process(std::uint32_t pid,
                                                       std::uint64_t address,
                                                       std::size_t size) noexcept;

/// Query process info class via indirect syscall (e.g. ProcessBasicInformation=0).
NTSTATUS syscall_query_process(HANDLE process, ULONG info_class, void* buffer,
                               ULONG buffer_size, ULONG* return_length) noexcept;

}  // namespace real::win

#endif
