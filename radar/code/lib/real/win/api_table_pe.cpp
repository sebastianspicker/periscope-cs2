// api_table_pe.cpp — PE/EAT parsing, OBF name table, batch resolution helpers.

#include "real/win/api_table_internal.hpp"

#if LR_PLATFORM_WINDOWS
#include <cstdio>
#include <new>

namespace real::win {
namespace api_detail {

uintptr_t find_module_by_name(const char* targetName) noexcept {
    if (!targetName || !*targetName) return 0;
    uintptr_t base = peb::find_module(targetName);
    if (base) return base;

    // Educational / portability fallback: GetModuleHandle is already in our IAT
    // (kernel32 is a dependency of this binary). Accept both "ntdll" and "ntdll.dll".
    char withDll[64]{};
    size_t n = 0;
    while (targetName[n] && n + 5 < sizeof(withDll)) {
        withDll[n] = targetName[n];
        ++n;
    }
    if (n + 4 < sizeof(withDll) && (n < 4 || std::strcmp(targetName + n - 4, ".dll") != 0)) {
        withDll[n++] = '.';
        withDll[n++] = 'd';
        withDll[n++] = 'l';
        withDll[n++] = 'l';
        withDll[n] = 0;
    } else {
        withDll[n] = 0;
    }

    HMODULE mod = ::GetModuleHandleA(withDll);
    if (!mod) mod = ::GetModuleHandleA(targetName);
    return reinterpret_cast<uintptr_t>(mod);
}

void* resolve_eat(uintptr_t moduleBase, const char* funcName) noexcept {
    if (!moduleBase || !funcName) return nullptr;

    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        moduleBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir.Size == 0 || dir.VirtualAddress == 0) {
        // No exports — try forwarded (e.g., some kernel32 exports from ntdll)
        return nullptr;
    }

    auto* exp = reinterpret_cast<const PeExportDir*>(
        moduleBase + dir.VirtualAddress);

    if (exp->NumberOfNames == 0 || exp->NumberOfFunctions == 0) return nullptr;

    auto* names = reinterpret_cast<const DWORD*>(moduleBase + exp->AddressOfNames);
    auto* ordinals = reinterpret_cast<const WORD*>(moduleBase + exp->AddressOfNameOrdinals);
    auto* functions = reinterpret_cast<const DWORD*>(moduleBase + exp->AddressOfFunctions);

    // Binary search the sorted name table
    int left = 0;
    int right = static_cast<int>(exp->NumberOfNames) - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        const char* midName = reinterpret_cast<const char*>(moduleBase + names[mid]);

        // Case-insensitive comparison
        int cmp = 0;
        const char* a = midName;
        const char* b = funcName;
        while (*a && *b) {
            char ca = *a;
            char cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca += 32;
            if (cb >= 'A' && cb <= 'Z') cb += 32;
            if (ca != cb) { cmp = ca - cb; break; }
            ++a; ++b;
        }
        if (!cmp) cmp = (*a ? 1 : (*b ? -1 : 0));
        if (cmp == 0) {
            WORD ordIdx = ordinals[mid];
            DWORD funcRva = functions[ordIdx];

            // Check for forwarded export
            if (funcRva >= dir.VirtualAddress &&
                funcRva < dir.VirtualAddress + dir.Size) {
                return nullptr;
            }

            // Verify resolved address is within module image bounds
            uintptr_t resolvedAddr = moduleBase + funcRva;
            uintptr_t imageEnd = moduleBase + nt->OptionalHeader.SizeOfImage;
            if (resolvedAddr < moduleBase || resolvedAddr >= imageEnd) {
                return nullptr;
            }

            return reinterpret_cast<void*>(resolvedAddr);
        }
        // midName < funcName → search right; midName > funcName → search left
        if (cmp < 0) left = mid + 1;
        else right = mid - 1;
    }

    return nullptr;
}

// Fallback resolver: PEB+EAT first, then GetModuleHandle/GetProcAddress.
void* resolve_fallback(const char* moduleName, const char* funcName) noexcept {
    uintptr_t modBase = find_module_by_name(moduleName);
    if (modBase) {
        void* result = resolve_eat(modBase, funcName);
        if (result) return result;
    }

    HMODULE mod = ::GetModuleHandleA(moduleName);
    if (!mod) mod = ::LoadLibraryA(moduleName);
    if (mod) {
        return reinterpret_cast<void*>(::GetProcAddress(mod, funcName));
    }
    return nullptr;
}


const char* get_name(NameId id) noexcept {
    static const char* s_names[] = {
        OBF("NtQuerySystemInformation"),
        OBF("NtReadVirtualMemory"),
        OBF("NtClose"),
        OBF("NtDuplicateObject"),
        OBF("NtOpenProcess"),
        OBF("NtDelayExecution"),
        OBF("NtFakeReserved1"),
        OBF("NtFakeReserved2"),
        OBF("DuplicateHandle"),
        OBF("CloseHandle"),
        OBF("GetProcessId"),
        OBF("GetCurrentProcessId"),
        OBF("LoadLibraryA"),
        OBF("GetProcAddress"),
        OBF("FreeLibrary"),
        OBF("K32ReservedA"),
        OBF("K32ReservedB"),
        OBF("NtWriteVirtualMemory"),
        OBF("NtCreateSection"),
        OBF("NtMapViewOfSection"),
        OBF("NtUnmapViewOfSection"),
        OBF("NtSetInformationThread"),
        OBF("NtQueryInformationProcess"),
        OBF("NtCreateThreadEx"),
        OBF("NtResumeThread"),
        OBF("NtResumeProcess"),
        OBF("NtSuspendProcess"),
        OBF("NtWaitForSingleObject"),
        OBF("NtOpenKey"),
        OBF("NtCreateFile"),
        OBF("NtDeviceIoControlFile"),
        OBF("NtQueryVirtualMemory"),
        OBF("NtProtectVirtualMemory"),
        OBF("RtlAdjustPrivilege"),
        OBF("RtlGetVersion"),
        OBF("RtlZeroMemory"),
        OBF("LdrRegisterDllNotification"),
        OBF("LdrUnregisterDllNotification"),
        OBF("NtDecoyResolveX"),
        OBF("NtDecoyResolveY"),
        OBF("OpenProcess"),
        OBF("ReadProcessMemory"),
        OBF("WriteProcessMemory"),
        OBF("CreateFileMappingA"),
        OBF("OpenFileMappingA"),
        OBF("MapViewOfFile"),
        OBF("UnmapViewOfFile"),
        OBF("CreateRemoteThread"),
        OBF("VirtualAllocEx"),
        OBF("VirtualFreeEx"),
        OBF("VirtualProtect"),
        OBF("VirtualQuery"),
        OBF("CreateToolhelp32Snapshot"),
        OBF("Process32FirstW"),
        OBF("Process32NextW"),
        OBF("Module32First"),
        OBF("Module32Next"),
        OBF("GetSystemTimes"),
        OBF("GlobalMemoryStatusEx"),
        OBF("QueryPerformanceCounter"),
        OBF("QueryPerformanceFrequency"),
        OBF("Sleep"),
        OBF("SetThreadPriority"),
        OBF("GetExitCodeProcess"),
        OBF("TerminateProcess"),
        OBF("EmptyWorkingSet"),
        OBF("CreateMutexA"),
        OBF("CreateFileA"),
        OBF("CreateFileW"),
        OBF("OpenProcessToken"),
        OBF("GetTokenInformation"),
        OBF("ReadFile"),
        OBF("WriteFile"),
        OBF("DeviceIoControl"),
        OBF("GetModuleBaseNameA"),
        OBF("GetModuleFileNameExA"),
        OBF("AddVectoredExceptionHandler"),
        OBF("RemoveVectoredExceptionHandler"),
        OBF("K32DecoyAlpha"),
        OBF("K32DecoyBeta"),
        OBF("RegisterClassExA"),
        OBF("CreateWindowExA"),
        OBF("SetWindowPos"),
        OBF("SetLayeredWindowAttributes"),
        OBF("SetWindowDisplayAffinity"),
        OBF("GetWindowDisplayAffinity"),
        OBF("GetWindowRect"),
        OBF("MoveWindow"),
        OBF("PeekMessageA"),
        OBF("DefWindowProcA"),
        OBF("FindWindowA"),
        OBF("ShowWindow"),
        OBF("UpdateWindow"),
        OBF("User32Decoy1"),
        OBF("User32Decoy2"),
        OBF("EnumProcessModules"),
        OBF("EnumProcessModulesEx"),
        OBF("GetModuleInformation"),
        OBF("PsapiDecoy1"),
        OBF("PsapiDecoy2"),
        OBF("RegOpenKeyExW"),
        OBF("RegQueryValueExW"),
        OBF("RegCloseKey"),
        OBF("RegEnumValueW"),
        OBF("RegEnumKeyExW"),
        OBF("OpenSCManagerA"),
        OBF("OpenServiceA"),
        OBF("CreateServiceA"),
        OBF("StartServiceA"),
        OBF("ControlService"),
        OBF("DeleteService"),
        OBF("AdvapiDecoy1"),
        OBF("AdvapiDecoy2"),
        OBF("D3D11CreateDevice"),
        OBF("D3D11CreateDeviceAndSwapChain"),
        OBF("CreateDXGIFactory"),
        OBF("CreateDXGIFactory1"),
        OBF("CreateDXGIFactory2"),
        OBF("ntdll.dll"),
        OBF("kernel32"),
        OBF("user32"),
        OBF("psapi"),
        OBF("advapi32"),
        OBF("d3d11"),
        OBF("dxgi"),
    };
    return s_names[static_cast<int>(id)];
}

void scramble_batch_order(BatchEntry* entries, size_t count) noexcept {
    if (count < 2) return;
    uint64_t state = build::kXorKeySeed ^ static_cast<uint64_t>(count);
    for (size_t i = count; i > 1; --i) {
        state = state * 0x5851F42D4C957F2DULL + 0x14057B7EF767814FULL;
        size_t j = static_cast<size_t>(state % i);
        BatchEntry tmp = entries[i - 1];
        entries[i - 1] = entries[j];
        entries[j] = tmp;
    }
}

void resolve_module_batch(uintptr_t moduleBase, BatchEntry* entries,
                                  size_t count) noexcept {
    if (!count || !entries) return;

    // If no module base, try fallback for each entry
    if (!moduleBase) {
        for (size_t i = 0; i < count; ++i) {
            if (entries[i].moduleRequired) continue;
            if (!*entries[i].slot && entries[i].fallbackModule) {
                *entries[i].slot = resolve_fallback(entries[i].fallbackModule,
                                                     entries[i].name);
            }
        }
        return;
    }

    // Scramble resolution order to prevent deterministic fingerprinting
    scramble_batch_order(entries, count);

    // Parse PE headers once for this module
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(moduleBase);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        moduleBase + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir.Size == 0 || dir.VirtualAddress == 0) return;

    const auto* exp = reinterpret_cast<const PeExportDir*>(
        moduleBase + dir.VirtualAddress);
    if (exp->NumberOfNames == 0 || exp->NumberOfFunctions == 0) return;

    const auto* names = reinterpret_cast<const DWORD*>(
        moduleBase + exp->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const WORD*>(
        moduleBase + exp->AddressOfNameOrdinals);
    const auto* funcTable = reinterpret_cast<const DWORD*>(
        moduleBase + exp->AddressOfFunctions);

    // Resolve all functions in one pass using shared EAT data
    for (size_t i = 0; i < count; ++i) {
        if (*entries[i].slot) continue;

        int left = 0;
        int right = static_cast<int>(exp->NumberOfNames) - 1;

        while (left <= right) {
            int mid = left + (right - left) / 2;
            const char* midName = reinterpret_cast<const char*>(
                moduleBase + names[mid]);

            int cmp = 0;
            const char* a = midName;
            const char* b = entries[i].name;
            while (*a && *b) {
                char ca = *a, cb = *b;
                if (ca >= 'A' && ca <= 'Z') ca += 32;
                if (cb >= 'A' && cb <= 'Z') cb += 32;
                if (ca != cb) { cmp = ca - cb; break; }
                ++a; ++b;
            }
            if (!cmp) cmp = (*a ? 1 : (*b ? -1 : 0));

            if (cmp == 0) {
                WORD ordIdx = ordinals[mid];
                DWORD funcRva = funcTable[ordIdx];

                // Skip forwarded exports
                if (!(funcRva >= dir.VirtualAddress &&
                      funcRva < dir.VirtualAddress + dir.Size)) {
                    *entries[i].slot = reinterpret_cast<void*>(
                        moduleBase + funcRva);
                }
                break;
            }
            // midName < name → search right; midName > name → search left
            if (cmp < 0) left = mid + 1;
            else right = mid - 1;
        }

        // Fallback if unresolved
        if (!*entries[i].slot && entries[i].fallbackModule) {
            *entries[i].slot = resolve_fallback(entries[i].fallbackModule,
                                                 entries[i].name);
        }
    }
}

// Dummy storage slots for junk API resolution decoys
static void* s_junkSlots[24];
static size_t s_junkCount = 0;

void** junk_dummy() noexcept {
    return &s_junkSlots[s_junkCount++ % 24];
}

ModuleBases& module_bases() noexcept {
    static ModuleBases bases{};
    return bases;
}

}  // namespace api_detail
}  // namespace real::win

#endif  // LR_PLATFORM_WINDOWS
