#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Single canonical PEB structures for the entire project.
// All PEB walks must go through this header — no more duplicate definitions.

#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
#include "real/win/windows_h.hpp"

namespace real::win::peb {

#pragma pack(push, 8)
struct LdrDataTableEntry {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    void* DllBase;
    void* EntryPoint;
    uint32_t SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
};

struct LdrData {
    uint32_t Length;
    uint8_t Initialized;
    uint8_t _pad[3];
    void* SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
};

struct Peb {
    uint8_t InheritedAddressSpace;
    uint8_t ReadImageFileExecOptions;
    uint8_t BeingDebugged;
    uint8_t BitField;
    void* Mutant;
    void* ImageBaseAddress;
    LdrData* Ldr;
};
#pragma pack(pop)

inline Peb* get_peb() noexcept {
    return reinterpret_cast<Peb*>(__readgsqword(0x60));
}

inline uintptr_t find_module(const char* target_name) noexcept {
    auto* peb = get_peb();
    if (!peb || !peb->Ldr || !target_name) return 0;

    auto* list_head = &peb->Ldr->InLoadOrderModuleList;
    auto* entry = list_head->Flink;

    while (entry != list_head) {
        // InLoadOrderLinks is the first field — entry points at the module itself.
        auto* module = reinterpret_cast<LdrDataTableEntry*>(entry);

        if (module->DllBase && module->BaseDllName.Buffer && module->BaseDllName.Length >= 2) {
            // UNICODE_STRING is length-based (not null-terminated).
            const unsigned int len =
                static_cast<unsigned int>(module->BaseDllName.Length / sizeof(wchar_t));
            const wchar_t* name_buf = module->BaseDllName.Buffer;
            unsigned int start = 0;
            for (unsigned int i = 0; i < len; ++i) {
                if (name_buf[i] == L'\\' || name_buf[i] == L':') start = i + 1;
            }
            // Strip extension: match basename without ".dll"
            unsigned int name_len = 0;
            for (unsigned int i = start; i < len && name_len < 64; ++i) {
                if (name_buf[i] == L'.') break;
                ++name_len;
            }

            if (name_len > 0) {
                unsigned int ti = 0;
                for (; ti < name_len && target_name[ti]; ++ti) {
                    wchar_t a = name_buf[start + ti];
                    char b = target_name[ti];
                    if (a >= L'A' && a <= L'Z') a = static_cast<wchar_t>(a + 32);
                    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + 32);
                    if (static_cast<char>(a) != b) break;
                }
                if (ti == name_len && target_name[ti] == '\0') {
                    return reinterpret_cast<uintptr_t>(module->DllBase);
                }
            }
        }
        entry = entry->Flink;
    }
    return 0;
}

} // namespace real::win::peb
#endif
