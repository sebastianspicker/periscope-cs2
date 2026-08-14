// process_finder.cpp — CS2 process discovery implementation.
//
// All calls go through g_Api (PEB+EAT resolved API table).
// CS2 is opened with PROCESS_QUERY_LIMITED_INFORMATION only.
// The hijack reader (T0.4) provides VM_READ capability.
//
// Reference: Periscope prototype/src/product_bootstrap.cpp,
//            prototype/src/memory/reader_hijack.cpp

#include "real/cs2/process_cache.hpp"
#include "real/cs2/process_finder.hpp"
#include "real/win/api_table.hpp"
#include "real/win/syscall_helper.hpp"
#include "real/win/xorstr.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace real::cs2 {
namespace {

bool name_matches(const char* actual, const char* expected) {
#if LR_PLATFORM_WINDOWS
    while (*actual && *expected) {
        char a = *actual;
        char b = *expected;
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
        ++actual;
        ++expected;
    }
    return *actual == *expected;
#else
    return std::strcmp(actual, expected) == 0;
#endif
}

#if LR_PLATFORM_WINDOWS
// Read the PE headers of the image mapped at base and return its
// SizeOfImage (0 when the memory is not a valid image or unreadable).
size_t pe_image_size(HANDLE handle, uint64_t base) noexcept {
    IMAGE_DOS_HEADER dos{};
    SIZE_T bytesRead = 0;
    NTSTATUS st = real::win::syscall_direct_NtReadVirtualMemory(
        handle, reinterpret_cast<PVOID>(base), &dos, sizeof(dos), &bytesRead);
    if (st < 0 || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;

    IMAGE_NT_HEADERS64 nt{};
    st = real::win::syscall_direct_NtReadVirtualMemory(
        handle, reinterpret_cast<PVOID>(base + dos.e_lfanew),
        &nt, sizeof(nt), &bytesRead);
    if (st < 0 || nt.Signature != IMAGE_NT_SIGNATURE) return 0;

    return static_cast<size_t>(nt.OptionalHeader.SizeOfImage);
}

// Build an ASCII module name from a UTF-16 mapped filename (basename part).
std::string basename_from_wide(const wchar_t* raw, size_t chars) noexcept {
    const wchar_t* nameStart = raw;
    for (size_t i = 0; i < chars; ++i) {
        if (raw[i] == L'\\' || raw[i] == L'/') nameStart = &raw[i + 1];
    }
    std::string out;
    out.reserve(chars);
    for (const wchar_t* it = nameStart; it < raw + chars; ++it) {
        out.push_back(*it < 128 ? static_cast<char>(*it) : '?');
    }
    return out;
}
#endif

} // namespace

std::optional<Cs2ProcessInfo> find_cs2() noexcept {
#if LR_PLATFORM_WINDOWS
    const auto& list = get_cached_process_list();
    for (const auto& entry : list) {
        if (name_matches(entry.name.c_str(), OBF("cs2.exe"))) {
            return Cs2ProcessInfo{entry.pid};
        }
    }
    return std::nullopt;
#else
    (void)name_matches;
    return std::nullopt;
#endif
}

// Deprecation: open_cs2_limited exposes a PROCESS_QUERY_LIMITED_INFORMATION
// handle via OpenProcess, which is detectable by EDR/anti-cheat at T0.
// Use HijackReader::setup() instead for stealth handle duplication.
#if defined(LR_ENABLE_EDUCATIONAL_OPENPROCESS)
#pragma message("open_cs2_limited() compiled — this uses OpenProcess and is detectable. Prefer HijackReader::setup().")
#endif

std::optional<Cs2ProcessInfo> open_cs2_limited(uint32_t pid) noexcept {
    // T0 radar uses HijackReader exclusively to obtain a handle with VM_READ
    // through handle duplication. open_cs2_limited is only for educational
    // diagnostics when LR_ENABLE_EDUCATIONAL_OPENPROCESS is defined.
    //
    // DEPRECATED: Use HijackReader::setup() instead.
#if LR_PLATFORM_WINDOWS && defined(LR_ENABLE_EDUCATIONAL_OPENPROCESS)
    // WARNING: This function opens CS2 directly — a major detection risk.
    // This must NEVER be compiled into production/release builds.
    // Only use in educational/diagnostic contexts with LR_ENABLE_EDUCATIONAL_OPENPROCESS.
    // In the T0 model, all CS2 handles come from HijackReader via handle duplication.
#warning "open_cs2_limited opens CS2 directly — do NOT use in production builds (HijackReader provides the safe alternative)"
    auto& api = real::win::g_Api();
    if (!api.resolved) return std::nullopt;

#ifndef NDEBUG
    std::printf("[process] WARNING: open_cs2_limited opens a CS2 handle.\n");
    std::printf("[process] T0 hijack model: use HijackReader::setup() instead.\n");
    std::printf("[process] Returning limited-handle for legacy compatibility only.\n");
#endif

    // NOTE: api.OpenProcess is the PEB+EAT-resolved OpenProcess (kernel32).
    // This call MUST remain gated behind LR_ENABLE_EDUCATIONAL_OPENPROCESS —
    // it creates a detectable handle-table entry for CS2. Production builds
    // must use HijackReader (handle duplication) instead.
    HANDLE hProc = api.OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return std::nullopt;

    return Cs2ProcessInfo{pid, reinterpret_cast<uint64_t>(hProc)};
#else
    (void)pid;
    return std::nullopt;
#endif
}

std::vector<Cs2ModuleInfo> enumerate_modules(
    uint32_t pid, uint64_t hProc) noexcept
{
    // Two strategies with graceful fallback:
    //   Strategy 1 (preferred): EnumProcessModulesEx (LIST_MODULES_ALL) +
    //     GetModuleInformation + GetModuleBaseNameA, with correct_image_size()
    //     for the real SizeOfImage. Requires PROCESS_QUERY_INFORMATION plus
    //     PROCESS_VM_READ — the hijack reader handle provides VM_READ.
    //   Strategy 2 (fallback): walk the VAS with NtQueryVirtualMemory, dedupe
    //     distinct MEM_IMAGE allocations, parse the remote PE headers directly
    //     for SizeOfImage, and name modules from mapped filenames.
#if LR_PLATFORM_WINDOWS
    auto& api = real::win::g_Api();
    if (!api.resolved || hProc == 0) return {};
    api.ensure_resolved();

    const HANDLE handle = reinterpret_cast<HANDLE>(hProc);
    constexpr DWORD kListModulesAll = 0x03;  // LIST_MODULES_ALL
    constexpr size_t kMaxModules = 512;

    // Strategy 1: PSAPI module enumeration.
    if (api.EnumProcessModulesEx && api.GetModuleInformation &&
        api.GetModuleBaseNameA) {
      HMODULE modules[kMaxModules]{};
      DWORD needed = 0;
      if (api.EnumProcessModulesEx(handle, modules,
                                   static_cast<DWORD>(sizeof(modules)),
                                   &needed, kListModulesAll)) {
        const DWORD count = static_cast<DWORD>(std::min<size_t>(
            static_cast<size_t>(needed) / sizeof(HMODULE), kMaxModules));
        std::vector<Cs2ModuleInfo> result;
        result.reserve(count);
        for (DWORD i = 0; i < count; ++i) {
          MODULEINFO info{};
          if (!api.GetModuleInformation(handle, modules[i], &info,
                                        static_cast<DWORD>(sizeof(info)))) {
            continue;
          }
          char nameBuf[MAX_PATH]{};
          if (api.GetModuleBaseNameA(handle, modules[i], nameBuf,
                                     static_cast<DWORD>(sizeof(nameBuf))) == 0) {
            continue;
          }
          const uint64_t base = reinterpret_cast<uint64_t>(info.lpBaseOfDll);
          size_t size = correct_image_size(pid, hProc, base);
          if (size == 0) {
            size = static_cast<size_t>(info.SizeOfImage);
          }
          result.push_back(Cs2ModuleInfo{nameBuf, base, size});
        }
        return result;
      }
    }

    // Strategy 2: VAS walk for handles without PSAPI rights.
    if (!api.NtQueryVirtualMemory) return {};

    std::vector<uint64_t> bases;
    bases.reserve(64);

    uint64_t cursor = 0;
    constexpr uint64_t kUserSpaceEnd = 0x7FFFFFFFFFFFULL;  // 128 TiB user space
    while (cursor < kUserSpaceEnd && bases.size() < kMaxModules) {
      MEMORY_BASIC_INFORMATION mbi{};
      ULONG returned = 0;
      const NTSTATUS st = api.NtQueryVirtualMemory(
          handle, reinterpret_cast<PVOID>(cursor),
          0 /* MemoryBasicInformation */, &mbi,
          static_cast<ULONG>(sizeof(mbi)), &returned);
      if (st < 0) break;

      if (mbi.State == MEM_COMMIT && mbi.Type == MEM_IMAGE) {
        const uint64_t allocBase =
            reinterpret_cast<uint64_t>(mbi.AllocationBase);
        if (std::find(bases.begin(), bases.end(), allocBase) == bases.end()) {
          bases.push_back(allocBase);
        }
      }

      const uint64_t regionBase = reinterpret_cast<uint64_t>(mbi.BaseAddress);
      const uint64_t regionSize = static_cast<uint64_t>(mbi.RegionSize);
      const uint64_t regionEnd = regionBase + regionSize;
      if (regionSize == 0 || regionEnd <= regionBase) break;
      cursor = regionEnd;
    }

    std::vector<Cs2ModuleInfo> result;
    result.reserve(bases.size());
    for (const uint64_t base : bases) {
      const size_t imageSize = pe_image_size(handle, base);
      if (imageSize == 0) continue;

      // Prefer a descriptive name from the mapped filename; fall back to the
      // base address in hex when no filename can be obtained.
      std::string name;
      alignas(16) char infoBuf[512]{};
      ULONG infoLen = 0;
      const NTSTATUS nameSt = api.NtQueryVirtualMemory(
          handle, reinterpret_cast<PVOID>(base),
          2 /* MemoryMappedFilenameInformation */, infoBuf,
          static_cast<ULONG>(sizeof(infoBuf)), &infoLen);
      // MemoryMappedFilenameInformation returns a UNICODE_STRING (optionally
      // followed by the path buffer). Avoid the SDK-only struct typedef.
      if (nameSt >= 0 && infoLen >= sizeof(UNICODE_STRING)) {
        const auto* mmfi = reinterpret_cast<const UNICODE_STRING*>(infoBuf);
        if (mmfi->Length > 0) {
          const wchar_t* rawName = mmfi->Buffer;
          size_t nameChars = mmfi->Length / sizeof(wchar_t);
          const uintptr_t bufStart = reinterpret_cast<uintptr_t>(infoBuf);
          const uintptr_t bufEnd = bufStart + sizeof(infoBuf);
          const uintptr_t rawPtr = reinterpret_cast<uintptr_t>(rawName);
          if (rawName == nullptr || rawPtr < bufStart ||
              rawPtr + nameChars * sizeof(wchar_t) > bufEnd) {
            // The kernel may not point Buffer into our buffer; the string
            // data follows the UNICODE_STRING header by convention.
            rawName = reinterpret_cast<const wchar_t*>(
                bufStart + sizeof(UNICODE_STRING));
            nameChars = (bufEnd - bufStart - sizeof(UNICODE_STRING)) /
                        sizeof(wchar_t);
          }
          name = basename_from_wide(rawName, nameChars);
        }
      }
      if (name.empty()) {
        char hexBuf[32]{};
        std::snprintf(hexBuf, sizeof(hexBuf), "0x%llx",
                      static_cast<unsigned long long>(base));
        name = hexBuf;
      }

      result.push_back(Cs2ModuleInfo{name, base, imageSize});
    }
    return result;
#else
    (void)pid;
    (void)hProc;
    return {};
#endif
}

size_t correct_image_size(uint32_t pid, uint64_t hProc,
                           uint64_t baseAddr) noexcept
{
    // NOTE: The process handle (hProc) only has PROCESS_QUERY_LIMITED_INFORMATION
    // rights, NOT PROCESS_VM_READ. We cannot use NtReadVirtualMemory here.
    // Instead, we trust the SizeOfImage returned by GetModuleInformation
    // (via EnumProcessModulesEx), which is populated by the OS kernel without
    // needing VM_READ on the handle.
    //
    // If the caller has a handle with PROCESS_VM_READ, they can call this
    // to correct the image size. Otherwise, returns 0 (caller uses the
    // original SizeOfImage from GetModuleInformation).

#if LR_PLATFORM_WINDOWS
    // Try to read via API table NtReadVirtualMemory (requires VM_READ).
    // If this fails (expected for limited handles), we just return 0.
    auto& api = real::win::g_Api();
    if (!api.resolved) return 0;

    HANDLE handle = reinterpret_cast<HANDLE>(hProc);

    IMAGE_DOS_HEADER dos{};
    SIZE_T bytesRead = 0;
    NTSTATUS st = real::win::syscall_direct_NtReadVirtualMemory(handle,
        reinterpret_cast<PVOID>(baseAddr), &dos, sizeof(dos), &bytesRead);
    if (st < 0 || dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;

    IMAGE_NT_HEADERS64 nt{};
    st = real::win::syscall_direct_NtReadVirtualMemory(handle,
        reinterpret_cast<PVOID>(baseAddr + dos.e_lfanew),
        &nt, sizeof(nt), &bytesRead);
    if (st < 0 || nt.Signature != IMAGE_NT_SIGNATURE) return 0;

    return static_cast<size_t>(nt.OptionalHeader.SizeOfImage);
#else
    (void)pid; (void)hProc; (void)baseAddr;
    return 0;
#endif
}

bool is_cs2_alive(uint32_t pid) noexcept {
#if LR_PLATFORM_WINDOWS
    return pid_exists(pid);
#else
    (void)pid;
    return false;
#endif
}

} // namespace real::cs2
