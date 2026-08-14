#pragma once
#include <cstdint>

// Single canonical Export Address Table parser for the entire project.
// All EAT resolution must go through this header.

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
#include "real/win/windows_h.hpp"

namespace real::win::eat {

inline bool name_equal(const char* a, const char* b) noexcept {
    while (*a && *b) {
        char ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
        char cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if (ca != cb) return false;
        ++a; ++b;
    }
    return *a == *b;
}

inline void* resolve_export(uintptr_t module_base, const char* func_name) noexcept {
    if (!module_base || !func_name) return nullptr;

    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(module_base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (dir.Size == 0 || dir.VirtualAddress == 0) return nullptr;

    const auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(module_base + dir.VirtualAddress);
    if (exp->NumberOfNames == 0 || exp->NumberOfFunctions == 0) return nullptr;

    const auto* names = reinterpret_cast<const uint32_t*>(module_base + exp->AddressOfNames);
    const auto* ordinals = reinterpret_cast<const uint16_t*>(module_base + exp->AddressOfNameOrdinals);
    const auto* functions = reinterpret_cast<const uint32_t*>(module_base + exp->AddressOfFunctions);

    int left = 0, right = static_cast<int>(exp->NumberOfNames) - 1;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        const char* mid_name = reinterpret_cast<const char*>(module_base + names[mid]);

        int cmp = 0;
        const char* a = mid_name;
        const char* b = func_name;
        while (*a && *b) {
            char ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
            char cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
            if (ca != cb) { cmp = ca - cb; break; }
            ++a; ++b;
        }
        if (!cmp) cmp = (*a ? 1 : (*b ? -1 : 0));

        if (cmp == 0) {
            uint16_t ord_idx = ordinals[mid];
            uint32_t func_rva = functions[ord_idx];
            if (func_rva >= dir.VirtualAddress && func_rva < dir.VirtualAddress + dir.Size)
                return nullptr;
            return reinterpret_cast<void*>(module_base + func_rva);
        }
        // mid_name < target → search right; mid_name > target → search left
        if (cmp < 0) left = mid + 1;
        else right = mid - 1;
    }
    return nullptr;
}

} // namespace real::win::eat
#endif
