#include "real/win/api_integrity.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS

#include "real/win/api_table.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/windows_h.hpp"

#include <cstring>

namespace real::win {
namespace {

bool module_range(const char* name, uintptr_t& base, uintptr_t& end) {
  base = peb::find_module(name);
  if (!base) return false;
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
  end = base + nt->OptionalHeader.SizeOfImage;
  return true;
}

void push_finding(IntegrityReport& rep, const IntegrityFinding& f) {
  if (rep.finding_count < 32) {
    rep.findings[rep.finding_count++] = f;
  }
}

}  // namespace

HookKind classify_prologue(const std::uint8_t* code, std::size_t len) noexcept {
  if (!code || len < 1) return HookKind::Unknown;

  // Legitimate hot-patch / mov-edi-edi padding is not a hook.
  // jmp rel32 / FF 25 only count as hooks when they dominate the prologue
  // (not when appearing later as part of a normal function body).
  if (code[0] == 0xE9) return HookKind::InlineJmp;
  if (len >= 6 && code[0] == 0xFF && code[1] == 0x25) return HookKind::InlineJmp;
  // FF 15 is call [rip+disp] — common in delay-load stubs; only flag if
  // it is the absolute first instruction and not preceded by a real body.
  if (len >= 6 && code[0] == 0xFF && code[1] == 0x15) return HookKind::InlineCall;
  // E8 call rel32 alone is extremely common in normal code — do NOT flag.
  if (len >= 6 && code[0] == 0x68 && code[5] == 0xC3) return HookKind::PushRet;
  if (len >= 12 && code[0] == 0x48 && code[1] == 0xB8 && code[10] == 0xFF &&
      code[11] == 0xE0)
    return HookKind::InlineJmp;
  if (code[0] == 0xCC) return HookKind::Int3;
  return HookKind::None;
}

IntegrityFinding verify_pointer(const char* module, const char* function,
                                void* resolved) noexcept {
  IntegrityFinding f{};
  f.module = module ? module : "";
  f.function = function ? function : "";
  f.address = reinterpret_cast<std::uint64_t>(resolved);
  if (!resolved) {
    f.ok = false;
    f.hook = HookKind::Unknown;
    return f;
  }
  uintptr_t base = 0, end = 0;
  if (module && module_range(module, base, end)) {
    auto addr = reinterpret_cast<uintptr_t>(resolved);
    if (addr < base || addr >= end) {
      f.ok = false;
      f.hook = HookKind::OutsideModule;
      return f;
    }
  }
  f.hook = classify_prologue(reinterpret_cast<const std::uint8_t*>(resolved), 16);
  f.ok = (f.hook == HookKind::None);
  return f;
}

IntegrityReport verify_api_table() noexcept {
  IntegrityReport rep{};
  auto& api = g_Api();
  api.ensure_resolved();

  struct Item {
    const char* mod;
    const char* fn;
    void* ptr;
  };
  Item items[] = {
      {"ntdll", "NtOpenProcess", reinterpret_cast<void*>(api.NtOpenProcess)},
      {"ntdll", "NtReadVirtualMemory", reinterpret_cast<void*>(api.NtReadVirtualMemory)},
      {"ntdll", "NtClose", reinterpret_cast<void*>(api.NtClose)},
      {"ntdll", "NtQuerySystemInformation",
       reinterpret_cast<void*>(api.NtQuerySystemInformation)},
      {"ntdll", "NtDuplicateObject", reinterpret_cast<void*>(api.NtDuplicateObject)},
      {"ntdll", "NtDelayExecution", reinterpret_cast<void*>(api.NtDelayExecution)},
      {"ntdll", "NtQueryInformationProcess",
       reinterpret_cast<void*>(api.NtQueryInformationProcess)},
      {"kernel32", "CloseHandle", reinterpret_cast<void*>(api.CloseHandle)},
      {"kernel32", "DuplicateHandle", reinterpret_cast<void*>(api.DuplicateHandle)},
      {"kernel32", "GetCurrentProcessId",
       reinterpret_cast<void*>(api.GetCurrentProcessId)},
  };

  for (const auto& it : items) {
    ++rep.checked;
    auto f = verify_pointer(it.mod, it.fn, it.ptr);
    if (!f.ok) {
      ++rep.failed;
      if (f.hook != HookKind::None && f.hook != HookKind::Unknown) ++rep.hooked;
      push_finding(rep, f);
    }
  }
  return rep;
}

}  // namespace real::win

#else

namespace real::win {
HookKind classify_prologue(const std::uint8_t*, std::size_t) noexcept {
  return HookKind::None;
}
IntegrityFinding verify_pointer(const char*, const char*, void*) noexcept {
  return {};
}
IntegrityReport verify_api_table() noexcept { return {}; }
}  // namespace real::win

#endif
