// api_table.hpp — Full API table with PEB+EAT resolution (IAT proxy pattern).
//
// THIS is the single API resolution mechanism for the project. The legacy
// ntapi.hpp/cpp (NtApiTable) has been removed — all NT, kernel32, user32,
// D3D11/DXGI, PSAPI, and Advapi32 functions are resolved through ApiTable
// via PEB LdrWalk + manual EAT traversal.
//
// All ~80+ Windows/NT/DXGI API function pointers are resolved via pure
// PEB LdrWalk + manual Export Address Table (EAT) traversal — NO static
// IAT entries, NO GetModuleHandle, NO GetProcAddress in the release path.
//
// This defeats IAT-scanning detection and eliminates static import
// signatures that AC tools can fingerprint. The IAT proxy pattern routes
// all API resolution through PEB+EAT at runtime, leaving the binary's
// Import Address Table empty of sensitive functions. Memory scanners
// that walk the IAT to enumerate imports will find nothing.
//
// Reference: Periscope prototype/src/conceal/api_table.cpp (170 lines)

#pragma once

#include "real/error.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS

// Pull real Windows/NT types first so we never redefine SDK types
// (MSVC hard-errors on conflicting LARGE_INTEGER / WPARAM / CLIENT_ID, etc.).
#include "real/win/windows_h.hpp"

// Bulletproof PCLIENT_ID: some SDK winternl.h versions define CLIENT_ID but
// omit PCLIENT_ID. Define once here so api_table.hpp never fails to parse.
#if !defined(_PCLIENT_ID_DEFINED)
#define _PCLIENT_ID_DEFINED
typedef CLIENT_ID* PCLIENT_ID;
#endif

#include <cstdint>
#include <cstddef>

// Forward declaration for LDR_DLL_NOTIFICATION_DATA (used in PLDR_DLL_NOTIFICATION_FUNCTION callback)
struct LDR_DLL_NOTIFICATION_DATA;
using PCLDR_DLL_NOTIFICATION_DATA = const LDR_DLL_NOTIFICATION_DATA*;

// PIO_STATUS_BLOCK may already exist via winternl; keep a void* alias only if absent.
#if !defined(_IO_STATUS_BLOCK_DEFINED) && !defined(__PIO_STATUS_BLOCK_DEFINED)
// winternl provides IO_STATUS_BLOCK; if PIO_STATUS_BLOCK is missing, alias it.
#ifndef PIO_STATUS_BLOCK
using PIO_STATUS_BLOCK = IO_STATUS_BLOCK*;
#endif
#endif


// Function pointer typedefs for all resolved APIs
namespace real::win {

/// @brief Runtime-resolved API table containing pointers to all Windows/NT/DXGI
///        functions used by Periscope-level features.
///
/// All pointers resolved via PEB LdrWalk + manual EAT traversal at startup.
/// Module and function names passed through OBF() compile-time XOR obfuscation.
struct ApiTable {
    // ====================================================================
    // NT API (ntdll.dll) — 22 functions
    // ====================================================================
    NTSTATUS (NTAPI* NtOpenProcess)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PCLIENT_ID);
    NTSTATUS (NTAPI* NtReadVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
    NTSTATUS (NTAPI* NtWriteVirtualMemory)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
    NTSTATUS (NTAPI* NtClose)(HANDLE);
    NTSTATUS (NTAPI* NtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
    NTSTATUS (NTAPI* NtDuplicateObject)(HANDLE, HANDLE, HANDLE, PHANDLE, ACCESS_MASK, ULONG, ULONG);
    NTSTATUS (NTAPI* NtCreateSection)(PHANDLE, ACCESS_MASK, PVOID, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
    NTSTATUS (NTAPI* NtMapViewOfSection)(HANDLE, HANDLE, PVOID*, ULONG, SIZE_T, PLARGE_INTEGER, PSIZE_T, ULONG, ULONG, ULONG);
    NTSTATUS (NTAPI* NtUnmapViewOfSection)(HANDLE, PVOID);
    NTSTATUS (NTAPI* NtDelayExecution)(BOOL, PLARGE_INTEGER);
    NTSTATUS (NTAPI* NtSetInformationThread)(HANDLE, ULONG, PVOID, ULONG);
    NTSTATUS (NTAPI* NtQueryInformationProcess)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    NTSTATUS (NTAPI* NtCreateThreadEx)(PHANDLE, ACCESS_MASK, PVOID, HANDLE, PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);
    NTSTATUS (NTAPI* NtResumeThread)(HANDLE, PULONG);
    NTSTATUS (NTAPI* NtResumeProcess)(HANDLE);
    NTSTATUS (NTAPI* NtSuspendProcess)(HANDLE);
    NTSTATUS (NTAPI* NtWaitForSingleObject)(HANDLE, BOOLEAN, PLARGE_INTEGER);
    NTSTATUS (NTAPI* NtOpenKey)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
    NTSTATUS (NTAPI* NtCreateFile)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
    NTSTATUS (NTAPI* NtDeviceIoControlFile)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, ULONG, PVOID, ULONG, PVOID, ULONG);
    NTSTATUS (NTAPI* NtQueryVirtualMemory)(HANDLE, PVOID, ULONG, PVOID, ULONG, PULONG);
    NTSTATUS (NTAPI* NtProtectVirtualMemory)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
    NTSTATUS (NTAPI* RtlAdjustPrivilege)(ULONG, BOOL, BOOL, PBOOL);

    // ====================================================================
    // Kernel32 — 30 functions
    // ====================================================================
    // DEPRECATED — Educational use ONLY. Never use in production builds.
    // OpenProcess creates an immediately-detectable handle in the system
    // handle table. All target process handles must come through HijackReader
    // (handle duplication from a donor process). This slot exists solely for
    // educational/demonstration code paths.
    HANDLE  (WINAPI* OpenProcess)(DWORD, BOOL, DWORD);
    BOOL    (WINAPI* CloseHandle)(HANDLE);
    // DEPRECATED — Educational use ONLY. Never use in production builds.
    // ReadProcessMemory is detectable via API call monitoring and handle
    // table enumeration. For self-read operations, use NtReadVirtualMemory
    // with NtCurrentProcess() pseudo-handle instead.
    BOOL    (WINAPI* ReadProcessMemory)(HANDLE, LPCVOID, LPVOID, SIZE_T, PSIZE_T);
    BOOL    (WINAPI* WriteProcessMemory)(HANDLE, LPVOID, LPCVOID, SIZE_T, PSIZE_T);
    HANDLE  (WINAPI* CreateFileMappingA)(HANDLE, PVOID, DWORD, DWORD, DWORD, LPCSTR);
    HANDLE  (WINAPI* OpenFileMappingA)(DWORD, BOOL, LPCSTR);
    LPVOID  (WINAPI* MapViewOfFile)(HANDLE, DWORD, DWORD, DWORD, SIZE_T);
    BOOL    (WINAPI* UnmapViewOfFile)(LPCVOID);
    HANDLE  (WINAPI* CreateRemoteThread)(HANDLE, PVOID, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, PDWORD);
    HANDLE  (WINAPI* VirtualAllocEx)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
    BOOL    (WINAPI* VirtualFreeEx)(HANDLE, LPVOID, SIZE_T, DWORD);
    BOOL    (WINAPI* VirtualProtect)(LPVOID, SIZE_T, DWORD, PDWORD);
    SIZE_T  (WINAPI* VirtualQuery)(LPCVOID, void*, SIZE_T);
    BOOL    (WINAPI* DuplicateHandle)(HANDLE, HANDLE, HANDLE, PHANDLE, DWORD, BOOL, DWORD);
    HANDLE  (WINAPI* CreateToolhelp32Snapshot)(DWORD, DWORD);
    BOOL    (WINAPI* Process32FirstW)(HANDLE, void*);
    BOOL    (WINAPI* Process32NextW)(HANDLE, void*);
    BOOL    (WINAPI* Module32First)(HANDLE, void*);
    BOOL    (WINAPI* Module32Next)(HANDLE, void*);
    DWORD   (WINAPI* GetCurrentProcessId)();
    DWORD   (WINAPI* GetProcessId)(HANDLE);
    BOOL    (WINAPI* GetSystemTimes)(void*, void*, void*);
    BOOL    (WINAPI* GlobalMemoryStatusEx)(void*);
    BOOL    (WINAPI* QueryPerformanceCounter)(PLARGE_INTEGER);
    BOOL    (WINAPI* QueryPerformanceFrequency)(PLARGE_INTEGER);
    void    (WINAPI* Sleep)(DWORD);
    BOOL    (WINAPI* SetThreadPriority)(HANDLE, int);
    BOOL    (WINAPI* GetExitCodeProcess)(HANDLE, PDWORD);
    BOOL    (WINAPI* TerminateProcess)(HANDLE, UINT);
    BOOL    (WINAPI* EmptyWorkingSet)(HANDLE);
    HANDLE  (WINAPI* CreateMutexA)(void*, BOOL, LPCSTR);
    HANDLE  (WINAPI* CreateFileA)(LPCSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
    HANDLE  (WINAPI* CreateFileW)(LPCWSTR, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
    BOOL    (WINAPI* ReadFile)(HANDLE, LPVOID, DWORD, PDWORD, void*);
    BOOL    (WINAPI* WriteFile)(HANDLE, LPCVOID, DWORD, PDWORD, void*);
    BOOL    (WINAPI* DeviceIoControl)(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, PDWORD, void*);
    DWORD   (WINAPI* GetModuleBaseNameA)(HANDLE, HMODULE, LPSTR, DWORD);
    DWORD   (WINAPI* GetModuleFileNameExA)(HANDLE, HMODULE, LPSTR, DWORD);
    HMODULE (WINAPI* LoadLibraryA)(LPCSTR);
    void*   (WINAPI* GetProcAddress)(HMODULE, LPCSTR);
    BOOL    (WINAPI* FreeLibrary)(HMODULE);
    BOOL    (WINAPI* OpenProcessToken)(HANDLE, DWORD, PHANDLE);
    BOOL    (WINAPI* GetTokenInformation)(HANDLE, DWORD, LPVOID, DWORD, PDWORD);
    PVOID   (WINAPI* AddVectoredExceptionHandler)(ULONG, PVECTORED_EXCEPTION_HANDLER);
    ULONG   (WINAPI* RemoveVectoredExceptionHandler)(PVOID);

    // ====================================================================
    // User32 — 12 functions
    // ====================================================================
    WORD    (WINAPI* RegisterClassExA)(const void*);
    HWND    (WINAPI* CreateWindowExA)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, void*, HANDLE, LPVOID);
    BOOL    (WINAPI* SetWindowPos)(HWND, HWND, int, int, int, int, UINT);
    BOOL    (WINAPI* SetLayeredWindowAttributes)(HWND, DWORD, BYTE, DWORD);
    BOOL    (WINAPI* SetWindowDisplayAffinity)(HWND, DWORD);
    BOOL    (WINAPI* GetWindowDisplayAffinity)(HWND, PDWORD);
    BOOL    (WINAPI* GetWindowRect)(HWND, void*);
    BOOL    (WINAPI* MoveWindow)(HWND, int, int, int, int, BOOL);
    BOOL    (WINAPI* PeekMessageA)(void*, HWND, UINT, UINT, UINT);
    LRESULT (WINAPI* DefWindowProcA)(HWND, UINT, WPARAM, LPARAM);
    HWND    (WINAPI* FindWindowA)(LPCSTR, LPCSTR);
    BOOL    (WINAPI* ShowWindow)(HWND, int);
    BOOL    (WINAPI* UpdateWindow)(HWND);

    // ====================================================================
    // D3D11 / DXGI — 6 functions
    // ====================================================================
    HRESULT (WINAPI* D3D11CreateDevice)(void*, DWORD, void*, UINT, void*, UINT, DWORD, void*, void*, void*);
    HRESULT (WINAPI* D3D11CreateDeviceAndSwapChain)(void*, DWORD, void*, UINT, void*, UINT, DWORD, void*, void*, void*, void*);
    HRESULT (WINAPI* CreateDXGIFactory)(void*, void*, void*);
    HRESULT (WINAPI* CreateDXGIFactory1)(void*, void*, void*);
    HRESULT (WINAPI* CreateDXGIFactory2)(UINT, void*, void*);

    // ====================================================================
    // PSAPI — 3 functions
    // ====================================================================
    BOOL    (WINAPI* EnumProcessModules)(HANDLE, HMODULE*, DWORD, PDWORD);
    BOOL    (WINAPI* EnumProcessModulesEx)(HANDLE, HMODULE*, DWORD, PDWORD, DWORD);
    BOOL    (WINAPI* GetModuleInformation)(HANDLE, HMODULE, void*, DWORD);

    // ====================================================================
    // Advapi32 — 8 functions
    // ====================================================================
    LONG    (WINAPI* RegOpenKeyExW)(HKEY, LPCWSTR, DWORD, ACCESS_MASK, PHKEY);
    LONG    (WINAPI* RegQueryValueExW)(HKEY, LPCWSTR, PDWORD, PDWORD, BYTE*, PDWORD);
    LONG    (WINAPI* RegCloseKey)(HKEY);
    LONG    (WINAPI* RegEnumValueW)(HKEY, DWORD, LPWSTR, PDWORD, PDWORD, PDWORD, PBYTE, PDWORD);
    LONG    (WINAPI* RegEnumKeyExW)(HKEY, DWORD, LPWSTR, PDWORD, PDWORD, LPWSTR, PDWORD, void*);
    HANDLE  (WINAPI* OpenSCManagerA)(LPCSTR, LPCSTR, DWORD);
    HANDLE  (WINAPI* OpenServiceA)(HANDLE, LPCSTR, DWORD);
    HANDLE  (WINAPI* CreateServiceA)(HANDLE, LPCSTR, LPCSTR, DWORD, DWORD, DWORD, DWORD, LPCSTR, LPCSTR, PDWORD, LPCSTR, LPCSTR, LPCSTR);
    BOOL    (WINAPI* StartServiceA)(HANDLE, DWORD, LPCSTR*);
    BOOL    (WINAPI* ControlService)(HANDLE, DWORD, void*);
    BOOL    (WINAPI* DeleteService)(HANDLE);

    // ====================================================================
    // ntdll.dll utility functions
    // ====================================================================
    void    (NTAPI* RtlGetVersion)(void*);
    void    (NTAPI* RtlZeroMemory)(PVOID, SIZE_T);
    NTSTATUS(NTAPI* LdrRegisterDllNotification)(ULONG Flags, PVOID NotificationFunction, PVOID Context, PVOID* Cookie);
    NTSTATUS(NTAPI* LdrUnregisterDllNotification)(PVOID Cookie);

    // PLDR_DLL_NOTIFICATION_FUNCTION — callback type for LdrRegisterDllNotification
    // Use void (not VOID) so this is valid even if Windows macros are stripped.
    using PLDR_DLL_NOTIFICATION_FUNCTION = void (NTAPI*)(ULONG NotificationReason, PCLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context);


    // Status
    bool resolved{false};
    bool critical_resolved{false};
    bool fully_resolved{false};

    /// Resolve all function pointers from loaded modules using PEB+EAT.
    /// Phase 1: Resolve critical subset (NtQuerySystemInformation,
    /// NtReadVirtualMemory, NtClose, NtDuplicateObject)
    /// These are needed immediately for hijack reader setup.
    /// Phase 2: Remaining functions are resolved on-demand via ensure_resolved()
    /// This spreads the detection surface over time.
    bool resolve() noexcept;

    /// Re-resolve all API pointers from fresh EAT lookups.
    /// Useful for defeating pointer-scraping memory scans after many
    /// ensure_resolved() calls, or for recovering from API hook injection.
    bool re_resolve() noexcept;

    /// Increment call counter and re-resolve if threshold exceeded.
    bool check_and_refresh() noexcept;

    /// Resolve any remaining unresolved functions on first use.
    bool ensure_resolved() noexcept;

    uint32_t call_count{0};
    static constexpr uint32_t kMaxCallsBeforeRefresh = 10000;

private:
    bool resolve_ntdll_batch() noexcept;
    bool resolve_kernel32_batch() noexcept;
    bool resolve_user32_batch() noexcept;
    bool resolve_psapi_batch() noexcept;
    bool resolve_advapi32_batch() noexcept;
    bool resolve_d3d11_batch() noexcept;
    bool resolve_dxgi_batch() noexcept;
};

/// Global API table singleton. Initialize once at startup.
ApiTable& g_Api() noexcept;

} // namespace real::win

#endif // LR_PLATFORM_WINDOWS

