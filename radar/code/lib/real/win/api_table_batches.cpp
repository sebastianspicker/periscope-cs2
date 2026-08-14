// api_table_batches.cpp — Per-module EAT resolve batches for ApiTable.

#include "real/win/api_table_internal.hpp"

#if LR_PLATFORM_WINDOWS

namespace real::win {
using namespace api_detail;

bool ApiTable::resolve_ntdll_batch() noexcept {
    api_detail::BatchEntry funcs[] = {
        { api_detail::get_name(kNtWriteVirtualMemory), reinterpret_cast<void**>(&this->NtWriteVirtualMemory), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtCreateSection), reinterpret_cast<void**>(&this->NtCreateSection), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtMapViewOfSection), reinterpret_cast<void**>(&this->NtMapViewOfSection), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtUnmapViewOfSection), reinterpret_cast<void**>(&this->NtUnmapViewOfSection), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtDelayExecution), reinterpret_cast<void**>(&this->NtDelayExecution), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtSetInformationThread), reinterpret_cast<void**>(&this->NtSetInformationThread), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtQueryInformationProcess), reinterpret_cast<void**>(&this->NtQueryInformationProcess), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtCreateThreadEx), reinterpret_cast<void**>(&this->NtCreateThreadEx), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtResumeThread), reinterpret_cast<void**>(&this->NtResumeThread), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtResumeProcess), reinterpret_cast<void**>(&this->NtResumeProcess), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtSuspendProcess), reinterpret_cast<void**>(&this->NtSuspendProcess), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtWaitForSingleObject), reinterpret_cast<void**>(&this->NtWaitForSingleObject), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtOpenKey), reinterpret_cast<void**>(&this->NtOpenKey), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtCreateFile), reinterpret_cast<void**>(&this->NtCreateFile), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtDeviceIoControlFile), reinterpret_cast<void**>(&this->NtDeviceIoControlFile), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtQueryVirtualMemory), reinterpret_cast<void**>(&this->NtQueryVirtualMemory), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtProtectVirtualMemory), reinterpret_cast<void**>(&this->NtProtectVirtualMemory), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kRtlAdjustPrivilege), reinterpret_cast<void**>(&this->RtlAdjustPrivilege), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kRtlGetVersion), reinterpret_cast<void**>(&this->RtlGetVersion), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kRtlZeroMemory), reinterpret_cast<void**>(&this->RtlZeroMemory), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kLdrRegisterDllNotification), reinterpret_cast<void**>(&this->LdrRegisterDllNotification), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kLdrUnregisterDllNotification), reinterpret_cast<void**>(&this->LdrUnregisterDllNotification), api_detail::get_name(kModNtdll), false },
        { api_detail::get_name(kNtDecoyResolveX), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
        { api_detail::get_name(kNtDecoyResolveY), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
    };
    api_detail::resolve_module_batch(api_detail::module_bases().ntdll, funcs,
        sizeof(funcs) / sizeof(funcs[0]));
    return true;
}

bool ApiTable::resolve_kernel32_batch() noexcept {
    api_detail::BatchEntry funcs[] = {
        { api_detail::get_name(kOpenProcess), reinterpret_cast<void**>(&this->OpenProcess), api_detail::get_name(kModKernel32), false },
        // ReadProcessMemory is for external process reads only.
        // For self-read (own process memory), use NtReadVirtualMemory
        // with NtCurrentProcess() instead.
        { api_detail::get_name(kReadProcessMemory), reinterpret_cast<void**>(&this->ReadProcessMemory), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kWriteProcessMemory), reinterpret_cast<void**>(&this->WriteProcessMemory), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kCreateFileMappingA), reinterpret_cast<void**>(&this->CreateFileMappingA), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kOpenFileMappingA), reinterpret_cast<void**>(&this->OpenFileMappingA), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kMapViewOfFile), reinterpret_cast<void**>(&this->MapViewOfFile), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kUnmapViewOfFile), reinterpret_cast<void**>(&this->UnmapViewOfFile), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kCreateRemoteThread), reinterpret_cast<void**>(&this->CreateRemoteThread), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kVirtualAllocEx), reinterpret_cast<void**>(&this->VirtualAllocEx), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kVirtualFreeEx), reinterpret_cast<void**>(&this->VirtualFreeEx), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kVirtualProtect), reinterpret_cast<void**>(&this->VirtualProtect), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kVirtualQuery), reinterpret_cast<void**>(&this->VirtualQuery), api_detail::get_name(kModKernel32), false },
        // CreateToolhelp32Snapshot is used for non-CS2 process enumeration
        // only (educational/cross-reference). It must NEVER be called on
        // CS2 — the handle enumerator will detect it.
        { api_detail::get_name(kCreateToolhelp32Snapshot), reinterpret_cast<void**>(&this->CreateToolhelp32Snapshot), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kProcess32FirstW), reinterpret_cast<void**>(&this->Process32FirstW), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kProcess32NextW), reinterpret_cast<void**>(&this->Process32NextW), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kModule32First), reinterpret_cast<void**>(&this->Module32First), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kModule32Next), reinterpret_cast<void**>(&this->Module32Next), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kGetSystemTimes), reinterpret_cast<void**>(&this->GetSystemTimes), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kGlobalMemoryStatusEx), reinterpret_cast<void**>(&this->GlobalMemoryStatusEx), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kQueryPerformanceCounter), reinterpret_cast<void**>(&this->QueryPerformanceCounter), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kQueryPerformanceFrequency), reinterpret_cast<void**>(&this->QueryPerformanceFrequency), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kSleep), reinterpret_cast<void**>(&this->Sleep), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kSetThreadPriority), reinterpret_cast<void**>(&this->SetThreadPriority), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kGetExitCodeProcess), reinterpret_cast<void**>(&this->GetExitCodeProcess), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kTerminateProcess), reinterpret_cast<void**>(&this->TerminateProcess), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kEmptyWorkingSet), reinterpret_cast<void**>(&this->EmptyWorkingSet), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kCreateMutexA), reinterpret_cast<void**>(&this->CreateMutexA), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kCreateFileA), reinterpret_cast<void**>(&this->CreateFileA), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kCreateFileW), reinterpret_cast<void**>(&this->CreateFileW), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kOpenProcessToken), reinterpret_cast<void**>(&this->OpenProcessToken), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kGetTokenInformation), reinterpret_cast<void**>(&this->GetTokenInformation), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kReadFile), reinterpret_cast<void**>(&this->ReadFile), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kWriteFile), reinterpret_cast<void**>(&this->WriteFile), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kDeviceIoControl), reinterpret_cast<void**>(&this->DeviceIoControl), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kGetModuleBaseNameA), reinterpret_cast<void**>(&this->GetModuleBaseNameA), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kGetModuleFileNameExA), reinterpret_cast<void**>(&this->GetModuleFileNameExA), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kLoadLibraryA), reinterpret_cast<void**>(&this->LoadLibraryA), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kGetProcAddress), reinterpret_cast<void**>(&this->GetProcAddress), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kFreeLibrary), reinterpret_cast<void**>(&this->FreeLibrary), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kAddVectoredExceptionHandler), reinterpret_cast<void**>(&this->AddVectoredExceptionHandler), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kRemoveVectoredExceptionHandler), reinterpret_cast<void**>(&this->RemoveVectoredExceptionHandler), api_detail::get_name(kModKernel32), false },
        { api_detail::get_name(kK32DecoyAlpha), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
        { api_detail::get_name(kK32DecoyBeta), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
    };
    api_detail::resolve_module_batch(api_detail::module_bases().kernel32, funcs,
        sizeof(funcs) / sizeof(funcs[0]));
    return true;
}

bool ApiTable::resolve_user32_batch() noexcept {
    auto mod_str = ::win::obf::encrypted_string("user32");
    uintptr_t user32_base = api_detail::find_module_by_name(mod_str.decrypt());
    mod_str.re_encrypt();

    api_detail::BatchEntry funcs[] = {
        { api_detail::get_name(kRegisterClassExA), reinterpret_cast<void**>(&this->RegisterClassExA), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kCreateWindowExA), reinterpret_cast<void**>(&this->CreateWindowExA), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kSetWindowPos), reinterpret_cast<void**>(&this->SetWindowPos), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kSetLayeredWindowAttributes), reinterpret_cast<void**>(&this->SetLayeredWindowAttributes), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kSetWindowDisplayAffinity), reinterpret_cast<void**>(&this->SetWindowDisplayAffinity), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kGetWindowDisplayAffinity), reinterpret_cast<void**>(&this->GetWindowDisplayAffinity), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kGetWindowRect), reinterpret_cast<void**>(&this->GetWindowRect), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kMoveWindow), reinterpret_cast<void**>(&this->MoveWindow), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kPeekMessageA), reinterpret_cast<void**>(&this->PeekMessageA), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kDefWindowProcA), reinterpret_cast<void**>(&this->DefWindowProcA), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kFindWindowA), reinterpret_cast<void**>(&this->FindWindowA), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kShowWindow), reinterpret_cast<void**>(&this->ShowWindow), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kUpdateWindow), reinterpret_cast<void**>(&this->UpdateWindow), api_detail::get_name(kModUser32), false },
        { api_detail::get_name(kUser32Decoy1), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
        { api_detail::get_name(kUser32Decoy2), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
    };
    api_detail::resolve_module_batch(user32_base, funcs,
        sizeof(funcs) / sizeof(funcs[0]));
    return true;
}

bool ApiTable::resolve_psapi_batch() noexcept {
    auto mod_str = ::win::obf::encrypted_string("psapi");
    uintptr_t psapi_base = api_detail::find_module_by_name(mod_str.decrypt());
    mod_str.re_encrypt();

    api_detail::BatchEntry funcs[] = {
        { api_detail::get_name(kEnumProcessModules), reinterpret_cast<void**>(&this->EnumProcessModules), api_detail::get_name(kModPsapi), false },
        { api_detail::get_name(kEnumProcessModulesEx), reinterpret_cast<void**>(&this->EnumProcessModulesEx), api_detail::get_name(kModPsapi), false },
        { api_detail::get_name(kGetModuleInformation), reinterpret_cast<void**>(&this->GetModuleInformation), api_detail::get_name(kModPsapi), false },
        { api_detail::get_name(kPsapiDecoy1), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
        { api_detail::get_name(kPsapiDecoy2), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
    };
    api_detail::resolve_module_batch(psapi_base, funcs,
        sizeof(funcs) / sizeof(funcs[0]));
    return true;
}

bool ApiTable::resolve_advapi32_batch() noexcept {
    auto mod_str = ::win::obf::encrypted_string("advapi32");
    uintptr_t advapi32_base = api_detail::find_module_by_name(mod_str.decrypt());
    mod_str.re_encrypt();

    api_detail::BatchEntry funcs[] = {
        { api_detail::get_name(kRegOpenKeyExW), reinterpret_cast<void**>(&this->RegOpenKeyExW), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kRegQueryValueExW), reinterpret_cast<void**>(&this->RegQueryValueExW), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kRegCloseKey), reinterpret_cast<void**>(&this->RegCloseKey), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kRegEnumValueW), reinterpret_cast<void**>(&this->RegEnumValueW), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kRegEnumKeyExW), reinterpret_cast<void**>(&this->RegEnumKeyExW), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kOpenSCManagerA), reinterpret_cast<void**>(&this->OpenSCManagerA), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kOpenServiceA), reinterpret_cast<void**>(&this->OpenServiceA), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kCreateServiceA), reinterpret_cast<void**>(&this->CreateServiceA), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kStartServiceA), reinterpret_cast<void**>(&this->StartServiceA), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kControlService), reinterpret_cast<void**>(&this->ControlService), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kDeleteService), reinterpret_cast<void**>(&this->DeleteService), api_detail::get_name(kModAdvapi32), false },
        { api_detail::get_name(kAdvapiDecoy1), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
        { api_detail::get_name(kAdvapiDecoy2), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
    };
    api_detail::resolve_module_batch(advapi32_base, funcs,
        sizeof(funcs) / sizeof(funcs[0]));
    return true;
}

bool ApiTable::resolve_d3d11_batch() noexcept {
    auto mod_str = ::win::obf::encrypted_string("d3d11");
    uintptr_t d3d11_base = api_detail::find_module_by_name(mod_str.decrypt());
    mod_str.re_encrypt();

    api_detail::BatchEntry funcs[] = {
        { api_detail::get_name(kD3D11CreateDevice), reinterpret_cast<void**>(&this->D3D11CreateDevice), api_detail::get_name(kModD3d11), false },
        { api_detail::get_name(kD3D11CreateDeviceAndSwapChain), reinterpret_cast<void**>(&this->D3D11CreateDeviceAndSwapChain), api_detail::get_name(kModD3d11), false },
    };
    api_detail::resolve_module_batch(d3d11_base, funcs,
        sizeof(funcs) / sizeof(funcs[0]));
    return true;
}

bool ApiTable::resolve_dxgi_batch() noexcept {
    auto mod_str = ::win::obf::encrypted_string("dxgi");
    uintptr_t dxgi_base = api_detail::find_module_by_name(mod_str.decrypt());
    mod_str.re_encrypt();

    api_detail::BatchEntry funcs[] = {
        { api_detail::get_name(kCreateDXGIFactory), reinterpret_cast<void**>(&this->CreateDXGIFactory), api_detail::get_name(kModDxgi), false },
        { api_detail::get_name(kCreateDXGIFactory1), reinterpret_cast<void**>(&this->CreateDXGIFactory1), api_detail::get_name(kModDxgi), false },
        { api_detail::get_name(kCreateDXGIFactory2), reinterpret_cast<void**>(&this->CreateDXGIFactory2), api_detail::get_name(kModDxgi), false },
    };
    api_detail::resolve_module_batch(dxgi_base, funcs,
        sizeof(funcs) / sizeof(funcs[0]));
    return true;
}

}  // namespace real::win

#endif  // LR_PLATFORM_WINDOWS

