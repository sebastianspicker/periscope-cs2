// nt_decls.hpp — NT function declarations for educational/demo code.
// These create IAT entries in any translation unit that includes this
// header. Production code in adapters/real/ MUST NOT include this file.
// Production code resolves NT functions through peb_util.hpp + eat_util.hpp
// (ApiTable / SyscallTable), not through static IAT declarations.

#pragma once

#if LR_PLATFORM_WINDOWS

#include <windows.h>
#include "real/win/nt_types.hpp"

extern "C" {
#ifndef _NTOPENPROCESS_DEFINED
NTSTATUS NTAPI NtOpenProcess(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess,
                             POBJECT_ATTRIBUTES ObjectAttributes,
                             PCLIENT_ID ClientId);
#endif
NTSTATUS NTAPI NtReadVirtualMemory(HANDLE ProcessHandle, PVOID BaseAddress,
                                   PVOID Buffer, SIZE_T NumberOfBytesToRead,
                                   PSIZE_T NumberOfBytesRead);
NTSTATUS NTAPI NtSuspendProcess(HANDLE ProcessHandle);
NTSTATUS NTAPI NtResumeProcess(HANDLE ProcessHandle);
NTSTATUS NTAPI NtOpenKey(PHANDLE KeyHandle, ACCESS_MASK DesiredAccess,
                          POBJECT_ATTRIBUTES ObjectAttributes);
}

#endif
