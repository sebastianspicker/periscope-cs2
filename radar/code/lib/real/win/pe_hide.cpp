#include "real/win/pe_hide.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
#include "real/win/api_table.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/windows_h.hpp"

#include <cstdint>
#include <cstring>

namespace real::win {
namespace {
static uint8_t s_saved_header[4096];
static bool s_header_erased = false;
static uintptr_t s_module_base = 0;
static std::size_t s_saved_size = 0;
static DWORD s_original_protect = PAGE_READONLY;

uintptr_t get_current_module_base() {
  auto* peb = peb::get_peb();
  return peb ? reinterpret_cast<uintptr_t>(peb->ImageBaseAddress) : 0;
}

bool protect(void* addr, std::size_t len, DWORD new_prot, DWORD* old_out) {
  auto& api = g_Api();
  api.ensure_resolved();
  if (api.NtProtectVirtualMemory) {
    PVOID base = addr;
    SIZE_T region = len;
    ULONG old = 0;
    HANDLE self = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
    NTSTATUS st =
        api.NtProtectVirtualMemory(self, &base, &region, new_prot, &old);
    if (st >= 0) {
      *old_out = old;
      return true;
    }
  }
  DWORD old = 0;
  if (!::VirtualProtect(addr, len, new_prot, &old)) return false;
  *old_out = old;
  return true;
}

}  // namespace

void erase_pe_header() noexcept {
  if (s_header_erased) return;

  uintptr_t base = get_current_module_base();
  if (!base) return;
  s_module_base = base;

  // Save full first page so restore is exact
  s_saved_size = 4096;
  std::memcpy(s_saved_header, reinterpret_cast<void*>(base), s_saved_size);

  if (!protect(reinterpret_cast<void*>(base), 4096, PAGE_READWRITE,
               &s_original_protect)) {
    return;
  }

  auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
  // Zero MZ
  dos->e_magic = 0;
  // Zero PE signature if e_lfanew still valid from saved copy
  auto* saved_dos = reinterpret_cast<IMAGE_DOS_HEADER*>(s_saved_header);
  if (saved_dos->e_lfanew > 0 &&
      static_cast<std::size_t>(saved_dos->e_lfanew) + 4 < s_saved_size) {
    *reinterpret_cast<std::uint32_t*>(base + saved_dos->e_lfanew) = 0;
  }
  // Zero e_lfanew
  dos->e_lfanew = 0;

  DWORD tmp = 0;
  protect(reinterpret_cast<void*>(base), 4096, s_original_protect, &tmp);
  s_header_erased = true;
}

void restore_pe_header() noexcept {
  if (!s_header_erased || !s_module_base || s_saved_size == 0) return;

  DWORD old = 0;
  if (!protect(reinterpret_cast<void*>(s_module_base), s_saved_size, PAGE_READWRITE,
               &old)) {
    return;
  }
  std::memcpy(reinterpret_cast<void*>(s_module_base), s_saved_header, s_saved_size);
  DWORD tmp = 0;
  protect(reinterpret_cast<void*>(s_module_base), s_saved_size, s_original_protect, &tmp);
  s_header_erased = false;
}

bool pe_header_erased() noexcept { return s_header_erased; }

}  // namespace real::win

#else
namespace real::win {
void erase_pe_header() noexcept {}
void restore_pe_header() noexcept {}
bool pe_header_erased() noexcept { return false; }
}  // namespace real::win
#endif
