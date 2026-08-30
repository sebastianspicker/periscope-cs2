// syscall_helper.hpp — Direct syscall invocation for x64 Windows.
// Dynamically resolves SSNs from ntdll at runtime to build syscall stubs.
// Only available on x64 Windows.

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64

#include "real/win/windows_h.hpp"

#include <cstdint>

namespace real::win {

/// Resolved syscall number (SSN) for a Windows system call.
struct SyscallNumber {
  int number = -1;       // SSN value
  const char* name = nullptr;  // debug name
};

/// Runtime-resolved syscall table for key NT calls.
struct SyscallTable {
  SyscallNumber NtOpenProcess;
  SyscallNumber NtReadVirtualMemory;
  SyscallNumber NtClose;
  SyscallNumber NtQueryInformationProcess;
  SyscallNumber NtCreateFile;
  SyscallNumber NtDeviceIoControlFile;
  SyscallNumber NtSuspendProcess;
  SyscallNumber NtResumeProcess;
  SyscallNumber NtDuplicateObject;

  /// Resolve all SSNs from the current ntdll.dll on disk.
  Result<void> resolve_all();
};

/// Global syscall table (lazily initialized from ntapi).
SyscallTable& syscalls();

/// Direct syscall wrapper for NtReadVirtualMemory.
/// Resolves the SSN from the global syscall table and uses syscall_5.
NTSTATUS syscall_direct_NtReadVirtualMemory(HANDLE process, PVOID address,
                                            PVOID buffer, SIZE_T size,
                                            PSIZE_T bytesRead);

/// Direct syscall wrapper for NtDuplicateObject.
/// Resolves the SSN from the global syscall table and uses syscall_7.
NTSTATUS syscall_direct_NtDuplicateObject(HANDLE SourceProcessHandle,
                                          HANDLE SourceHandle,
                                          HANDLE TargetProcessHandle,
                                          PHANDLE TargetHandle,
                                          ACCESS_MASK DesiredAccess,
                                          ULONG HandleAttributes,
                                          ULONG Options);

/// Execute a direct syscall with 4 arguments.
/// Used for NtReadVirtualMemory-style calls.
NTSTATUS syscall_4(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4);

/// Execute a direct syscall with 5 arguments.
NTSTATUS syscall_5(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5);

/// Execute a direct syscall with 6 arguments.
NTSTATUS syscall_6(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                   std::uint64_t arg6);

/// Execute a direct syscall with 7 arguments.
NTSTATUS syscall_7(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                   std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                   std::uint64_t arg6, std::uint64_t arg7);

}  // namespace real::win

#if LR_COMPILER_MSVC
// MASM indirect syscall wrappers (syscall_msvc.asm)
extern "C" {
    extern void* g_syscall_gadget_ptr;
    NTSTATUS syscall_asm_4(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                           std::uint64_t arg3, std::uint64_t arg4);
    NTSTATUS syscall_asm_5(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                           std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5);
    NTSTATUS syscall_asm_6(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                           std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                           std::uint64_t arg6);
    NTSTATUS syscall_asm_7(int ssn, std::uint64_t arg1, std::uint64_t arg2,
                           std::uint64_t arg3, std::uint64_t arg4, std::uint64_t arg5,
                           std::uint64_t arg6, std::uint64_t arg7);
}
#endif

#endif  // LR_PLATFORM_WINDOWS && LR_ARCH_X64
