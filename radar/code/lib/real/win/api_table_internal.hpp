// api_table_internal.hpp — PE/EAT helpers and batch machinery for api_table*.cpp.
#pragma once

#include "real/win/api_table.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/timing.hpp"
#include "real/win/xorstr.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace real::win {
namespace api_detail {

struct PeExportDir {
    DWORD Characteristics;
    DWORD TimeDateStamp;
    WORD  MajorVersion;
    WORD  MinorVersion;
    DWORD Name;
    DWORD Base;
    DWORD NumberOfFunctions;
    DWORD NumberOfNames;
    DWORD AddressOfFunctions;
    DWORD AddressOfNames;
    DWORD AddressOfNameOrdinals;
};

enum NameId : int {
    kNtQuerySystemInformation,
    kNtReadVirtualMemory,
    kNtClose,
    kNtDuplicateObject,
    kNtOpenProcess,
    kNtDelayExecution,
    kNtFakeReserved1,
    kNtFakeReserved2,
    kDuplicateHandle,
    kCloseHandle,
    kGetProcessId,
    kGetCurrentProcessId,
    kLoadLibraryA,
    kGetProcAddress,
    kFreeLibrary,
    kK32ReservedA,
    kK32ReservedB,
    kNtWriteVirtualMemory,
    kNtCreateSection,
    kNtMapViewOfSection,
    kNtUnmapViewOfSection,
    kNtSetInformationThread,
    kNtQueryInformationProcess,
    kNtCreateThreadEx,
    kNtResumeThread,
    kNtResumeProcess,
    kNtSuspendProcess,
    kNtWaitForSingleObject,
    kNtOpenKey,
    kNtCreateFile,
    kNtDeviceIoControlFile,
    kNtQueryVirtualMemory,
    kNtProtectVirtualMemory,
    kRtlAdjustPrivilege,
    kRtlGetVersion,
    kRtlZeroMemory,
    kLdrRegisterDllNotification,
    kLdrUnregisterDllNotification,
    kNtDecoyResolveX,
    kNtDecoyResolveY,
    kOpenProcess,
    kReadProcessMemory,
    kWriteProcessMemory,
    kCreateFileMappingA,
    kOpenFileMappingA,
    kMapViewOfFile,
    kUnmapViewOfFile,
    kCreateRemoteThread,
    kVirtualAllocEx,
    kVirtualFreeEx,
    kVirtualProtect,
    kVirtualQuery,
    kCreateToolhelp32Snapshot,
    kProcess32FirstW,
    kProcess32NextW,
    kModule32First,
    kModule32Next,
    kGetSystemTimes,
    kGlobalMemoryStatusEx,
    kQueryPerformanceCounter,
    kQueryPerformanceFrequency,
    kSleep,
    kSetThreadPriority,
    kGetExitCodeProcess,
    kTerminateProcess,
    kEmptyWorkingSet,
    kCreateMutexA,
    kCreateFileA,
    kCreateFileW,
    kOpenProcessToken,
    kGetTokenInformation,
    kReadFile,
    kWriteFile,
    kDeviceIoControl,
    kGetModuleBaseNameA,
    kGetModuleFileNameExA,
    kAddVectoredExceptionHandler,
    kRemoveVectoredExceptionHandler,
    kK32DecoyAlpha,
    kK32DecoyBeta,
    kRegisterClassExA,
    kCreateWindowExA,
    kSetWindowPos,
    kSetLayeredWindowAttributes,
    kSetWindowDisplayAffinity,
    kGetWindowDisplayAffinity,
    kGetWindowRect,
    kMoveWindow,
    kPeekMessageA,
    kDefWindowProcA,
    kFindWindowA,
    kShowWindow,
    kUpdateWindow,
    kUser32Decoy1,
    kUser32Decoy2,
    kEnumProcessModules,
    kEnumProcessModulesEx,
    kGetModuleInformation,
    kPsapiDecoy1,
    kPsapiDecoy2,
    kRegOpenKeyExW,
    kRegQueryValueExW,
    kRegCloseKey,
    kRegEnumValueW,
    kRegEnumKeyExW,
    kOpenSCManagerA,
    kOpenServiceA,
    kCreateServiceA,
    kStartServiceA,
    kControlService,
    kDeleteService,
    kAdvapiDecoy1,
    kAdvapiDecoy2,
    kD3D11CreateDevice,
    kD3D11CreateDeviceAndSwapChain,
    kCreateDXGIFactory,
    kCreateDXGIFactory1,
    kCreateDXGIFactory2,
    kModNtdll,
    kModKernel32,
    kModUser32,
    kModPsapi,
    kModAdvapi32,
    kModD3d11,
    kModDxgi,
    kNameCount,
};

struct BatchEntry {
    const char* name;
    void** slot;
    const char* fallbackModule;
    bool moduleRequired;
};

struct ModuleBases {
    uintptr_t ntdll;
    uintptr_t kernel32;
    bool valid;
};

uintptr_t find_module_by_name(const char* targetName) noexcept;
void* resolve_eat(uintptr_t moduleBase, const char* funcName) noexcept;
void* resolve_fallback(const char* moduleName, const char* funcName) noexcept;
const char* get_name(NameId id) noexcept;
void scramble_batch_order(BatchEntry* entries, size_t count) noexcept;
void resolve_module_batch(uintptr_t moduleBase, BatchEntry* entries,
                          size_t count) noexcept;
void** junk_dummy() noexcept;

ModuleBases& module_bases() noexcept;

}  // namespace api_detail
}  // namespace real::win

#endif  // LR_PLATFORM_WINDOWS
