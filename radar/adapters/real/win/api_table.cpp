// api_table.cpp — ApiTable resolve orchestration and global g_Api().

#include "real/win/api_table_internal.hpp"

#if LR_PLATFORM_WINDOWS
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <new>

namespace real::win {
using namespace api_detail;

bool ApiTable::check_and_refresh() noexcept {
    ++call_count;
    if (call_count >= kMaxCallsBeforeRefresh) {
        call_count = 0;
        // Force full re-resolution on next ensure_resolved
        fully_resolved = false;
        // Re-resolve critical subset from fresh EAT lookups
        resolve();
        return true;
    }
    return true;
}

bool ApiTable::re_resolve() noexcept {
    // Zero all function pointers to force fresh EAT re-resolution.
    // Use offsetof(ApiTable, resolved) since all pointer fields precede
    // the status fields in the struct layout.
    auto saved_call_count = call_count;
    std::memset(this, 0, offsetof(ApiTable, resolved));
    call_count = saved_call_count;
    fully_resolved = false;
    critical_resolved = false;
    resolved = false;

    // Re-resolve using cached module bases (s_bases preserved)
    if (!resolve()) return false;
    ensure_resolved();
    return true;
}

bool ApiTable::resolve() noexcept {
    // Phase 1: Resolve critical subset needed for hijack reader setup
    {
        auto ntdll_str = ::win::obf::encrypted_string("ntdll");
        api_detail::module_bases().ntdll = api_detail::find_module_by_name(ntdll_str.decrypt());
        ntdll_str.re_encrypt();
    }
    {
        auto kernel32_str = ::win::obf::encrypted_string("kernel32");
        api_detail::module_bases().kernel32 = api_detail::find_module_by_name(kernel32_str.decrypt());
        kernel32_str.re_encrypt();
    }
    api_detail::module_bases().valid    = true;

    // Critical NT functions needed immediately
    {
        api_detail::BatchEntry funcs[] = {
            { api_detail::get_name(kNtQuerySystemInformation), reinterpret_cast<void**>(&this->NtQuerySystemInformation), api_detail::get_name(kModNtdll), false },
            { api_detail::get_name(kNtReadVirtualMemory), reinterpret_cast<void**>(&this->NtReadVirtualMemory), api_detail::get_name(kModNtdll), false },
            { api_detail::get_name(kNtClose), reinterpret_cast<void**>(&this->NtClose), api_detail::get_name(kModNtdll), false },
            { api_detail::get_name(kNtDuplicateObject), reinterpret_cast<void**>(&this->NtDuplicateObject), api_detail::get_name(kModNtdll), false },
            { api_detail::get_name(kNtOpenProcess), reinterpret_cast<void**>(&this->NtOpenProcess), api_detail::get_name(kModNtdll), false },
            { api_detail::get_name(kNtDelayExecution), reinterpret_cast<void**>(&this->NtDelayExecution), api_detail::get_name(kModNtdll), false },
            { api_detail::get_name(kNtFakeReserved1), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
            { api_detail::get_name(kNtFakeReserved2), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
        };
        api_detail::resolve_module_batch(api_detail::module_bases().ntdll, funcs,
            sizeof(funcs) / sizeof(funcs[0]));
    }

    // Timing jitter — small delay to break deterministic timing analysis
    {
        LARGE_INTEGER delay;
        delay.QuadPart = -10000LL; // 1ms base
        if (NtDelayExecution) NtDelayExecution(FALSE, &delay);
    }

    // Critical kernel32 functions needed immediately
    {
        api_detail::BatchEntry funcs[] = {
            { api_detail::get_name(kDuplicateHandle), reinterpret_cast<void**>(&this->DuplicateHandle), api_detail::get_name(kModKernel32), false },
            { api_detail::get_name(kCloseHandle), reinterpret_cast<void**>(&this->CloseHandle), api_detail::get_name(kModKernel32), false },
            { api_detail::get_name(kGetProcessId), reinterpret_cast<void**>(&this->GetProcessId), api_detail::get_name(kModKernel32), false },
            { api_detail::get_name(kGetCurrentProcessId), reinterpret_cast<void**>(&this->GetCurrentProcessId), api_detail::get_name(kModKernel32), false },
            { api_detail::get_name(kLoadLibraryA), reinterpret_cast<void**>(&this->LoadLibraryA), api_detail::get_name(kModKernel32), false },
            { api_detail::get_name(kGetProcAddress), reinterpret_cast<void**>(&this->GetProcAddress), api_detail::get_name(kModKernel32), false },
            { api_detail::get_name(kFreeLibrary), reinterpret_cast<void**>(&this->FreeLibrary), api_detail::get_name(kModKernel32), false },
            { api_detail::get_name(kK32ReservedA), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
            { api_detail::get_name(kK32ReservedB), reinterpret_cast<void**>(api_detail::junk_dummy()), nullptr, false },
        };
        api_detail::resolve_module_batch(api_detail::module_bases().kernel32, funcs,
            sizeof(funcs) / sizeof(funcs[0]));
    }

    // If EAT walk failed for critical APIs, fill from GetProcAddress.
    auto fill = [](void** slot, const char* mod, const char* name) {
        if (*slot) return;
        HMODULE h = ::GetModuleHandleA(mod);
        if (!h) h = ::LoadLibraryA(mod);
        if (h) *slot = reinterpret_cast<void*>(::GetProcAddress(h, name));
    };
    fill(reinterpret_cast<void**>(&NtQuerySystemInformation), "ntdll.dll", "NtQuerySystemInformation");
    fill(reinterpret_cast<void**>(&NtReadVirtualMemory), "ntdll.dll", "NtReadVirtualMemory");
    fill(reinterpret_cast<void**>(&NtClose), "ntdll.dll", "NtClose");
    fill(reinterpret_cast<void**>(&NtDuplicateObject), "ntdll.dll", "NtDuplicateObject");
    fill(reinterpret_cast<void**>(&NtOpenProcess), "ntdll.dll", "NtOpenProcess");
    fill(reinterpret_cast<void**>(&NtDelayExecution), "ntdll.dll", "NtDelayExecution");
    fill(reinterpret_cast<void**>(&DuplicateHandle), "kernel32.dll", "DuplicateHandle");
    fill(reinterpret_cast<void**>(&CloseHandle), "kernel32.dll", "CloseHandle");
    fill(reinterpret_cast<void**>(&GetProcessId), "kernel32.dll", "GetProcessId");
    fill(reinterpret_cast<void**>(&GetCurrentProcessId), "kernel32.dll", "GetCurrentProcessId");
    fill(reinterpret_cast<void**>(&LoadLibraryA), "kernel32.dll", "LoadLibraryA");
    fill(reinterpret_cast<void**>(&GetProcAddress), "kernel32.dll", "GetProcAddress");
    fill(reinterpret_cast<void**>(&FreeLibrary), "kernel32.dll", "FreeLibrary");
    fill(reinterpret_cast<void**>(&OpenProcess), "kernel32.dll", "OpenProcess");
    fill(reinterpret_cast<void**>(&ReadProcessMemory), "kernel32.dll", "ReadProcessMemory");
    fill(reinterpret_cast<void**>(&WriteProcessMemory), "kernel32.dll", "WriteProcessMemory");
    fill(reinterpret_cast<void**>(&CreateToolhelp32Snapshot), "kernel32.dll", "CreateToolhelp32Snapshot");
    fill(reinterpret_cast<void**>(&Process32FirstW), "kernel32.dll", "Process32FirstW");
    fill(reinterpret_cast<void**>(&Process32NextW), "kernel32.dll", "Process32NextW");
    fill(reinterpret_cast<void**>(&EnumProcessModules), "psapi.dll", "EnumProcessModules");
    // Prefer K32* exports when psapi forwards are unavailable.
    if (!EnumProcessModules) {
      fill(reinterpret_cast<void**>(&EnumProcessModules), "kernel32.dll", "K32EnumProcessModules");
    }
    fill(reinterpret_cast<void**>(&EnumProcessModulesEx), "psapi.dll", "EnumProcessModulesEx");
    if (!EnumProcessModulesEx) {
      fill(reinterpret_cast<void**>(&EnumProcessModulesEx), "kernel32.dll", "K32EnumProcessModulesEx");
    }
    fill(reinterpret_cast<void**>(&GetModuleInformation), "psapi.dll", "GetModuleInformation");
    if (!GetModuleInformation) {
      fill(reinterpret_cast<void**>(&GetModuleInformation), "kernel32.dll", "K32GetModuleInformation");
    }
    fill(reinterpret_cast<void**>(&GetModuleFileNameExA), "psapi.dll", "GetModuleFileNameExA");
    if (!GetModuleFileNameExA) {
      fill(reinterpret_cast<void**>(&GetModuleFileNameExA), "kernel32.dll", "K32GetModuleFileNameExA");
    }
    fill(reinterpret_cast<void**>(&GetModuleBaseNameA), "psapi.dll", "GetModuleBaseNameA");

    bool criticalOk =
        NtQuerySystemInformation && NtReadVirtualMemory && NtClose &&
        NtDuplicateObject && NtOpenProcess &&
        DuplicateHandle && CloseHandle && GetProcessId;

    critical_resolved = criticalOk;
    resolved = criticalOk;
    if (!criticalOk) {
        std::fprintf(stderr,
            "[api_table] resolve failed ntdll=%p kernel32=%p "
            "NtOpen=%p NtRead=%p Dup=%p Close=%p GetPid=%p\n",
            (void*)api_detail::module_bases().ntdll, (void*)api_detail::module_bases().kernel32,
            (void*)NtOpenProcess, (void*)NtReadVirtualMemory,
            (void*)DuplicateHandle, (void*)CloseHandle, (void*)GetProcessId);
    }
    return resolved;
}

bool ApiTable::ensure_resolved() noexcept {
    check_and_refresh();
    if (fully_resolved) return true;

    if (!resolve_ntdll_batch()) return false;
    if (!resolve_kernel32_batch()) return false;
    if (!resolve_user32_batch()) return false;
    if (!resolve_psapi_batch()) return false;
    if (!resolve_advapi32_batch()) return false;
    if (!resolve_d3d11_batch()) return false;
    if (!resolve_dxgi_batch()) return false;

    fully_resolved = true;
    return true;
}

ApiTable& g_Api() noexcept {
    static ApiTable table;
    static bool initialized = false;
    if (!initialized) {
        initialized = table.resolve();
    }
    return table;
}

}  // namespace real::win

#endif  // LR_PLATFORM_WINDOWS

