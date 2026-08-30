#include "real/win/ssn_resolve.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64

#include "real/win/peb_util.hpp"
#include "real/win/windows_h.hpp"
#include "real/win/xorstr.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace real::win {
namespace {

struct ExportEntry {
  char name[128];
  const std::uint8_t* address;
  DWORD rva;
};

bool name_eq_ci(const char* a, const char* b) noexcept {
  while (*a && *b) {
    char ca = *a >= 'A' && *a <= 'Z' ? static_cast<char>(*a + 32) : *a;
    char cb = *b >= 'A' && *b <= 'Z' ? static_cast<char>(*b + 32) : *b;
    if (ca != cb) return false;
    ++a;
    ++b;
  }
  return *a == *b;
}

// Convert NtFoo <-> ZwFoo for dual-name lookups.
void to_zw_name(const char* nt, char* out, std::size_t out_cap) noexcept {
  if (!nt || out_cap < 3) {
    if (out_cap) out[0] = 0;
    return;
  }
  if ((nt[0] == 'N' || nt[0] == 'n') && (nt[1] == 't' || nt[1] == 'T')) {
    out[0] = 'Z';
    out[1] = 'w';
    std::size_t i = 2;
    for (; nt[i] && i + 1 < out_cap; ++i) out[i] = nt[i];
    out[i] = 0;
  } else {
    std::size_t i = 0;
    for (; nt[i] && i + 1 < out_cap; ++i) out[i] = nt[i];
    out[i] = 0;
  }
}

void to_nt_name(const char* zw, char* out, std::size_t out_cap) noexcept {
  if (!zw || out_cap < 3) {
    if (out_cap) out[0] = 0;
    return;
  }
  if ((zw[0] == 'Z' || zw[0] == 'z') && (zw[1] == 'w' || zw[1] == 'W')) {
    out[0] = 'N';
    out[1] = 't';
    std::size_t i = 2;
    for (; zw[i] && i + 1 < out_cap; ++i) out[i] = zw[i];
    out[i] = 0;
  } else {
    std::size_t i = 0;
    for (; zw[i] && i + 1 < out_cap; ++i) out[i] = zw[i];
    out[i] = 0;
  }
}

bool collect_zw_exports(uintptr_t ntdll, std::vector<ExportEntry>& out) {
  out.clear();
  const auto* base = reinterpret_cast<const std::uint8_t*>(ntdll);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
  const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (!dir.VirtualAddress || !dir.Size) return false;

  const auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dir.VirtualAddress);
  const auto* names = reinterpret_cast<const DWORD*>(base + exp->AddressOfNames);
  const auto* ords = reinterpret_cast<const WORD*>(base + exp->AddressOfNameOrdinals);
  const auto* funcs = reinterpret_cast<const DWORD*>(base + exp->AddressOfFunctions);

  out.reserve(exp->NumberOfNames / 4);
  for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
    const char* name = reinterpret_cast<const char*>(base + names[i]);
    // Zw* exports share SSNs with Nt* and are contiguous in address order.
    if (name[0] != 'Z' || name[1] != 'w') continue;
    DWORD rva = funcs[ords[i]];
    // Skip forwards
    if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size) continue;
    ExportEntry e{};
    std::size_t n = 0;
    for (; name[n] && n + 1 < sizeof(e.name); ++n) e.name[n] = name[n];
    e.name[n] = 0;
    e.rva = rva;
    e.address = base + rva;
    out.push_back(e);
  }
  std::sort(out.begin(), out.end(),
            [](const ExportEntry& a, const ExportEntry& b) { return a.address < b.address; });
  return !out.empty();
}

const std::uint8_t* find_export(uintptr_t ntdll, const char* name) {
  const auto* base = reinterpret_cast<const std::uint8_t*>(ntdll);
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
  const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (!dir.VirtualAddress) return nullptr;
  const auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + dir.VirtualAddress);
  const auto* names = reinterpret_cast<const DWORD*>(base + exp->AddressOfNames);
  const auto* ords = reinterpret_cast<const WORD*>(base + exp->AddressOfNameOrdinals);
  const auto* funcs = reinterpret_cast<const DWORD*>(base + exp->AddressOfFunctions);

  for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
    const char* en = reinterpret_cast<const char*>(base + names[i]);
    if (!name_eq_ci(en, name)) continue;
    DWORD rva = funcs[ords[i]];
    if (rva >= dir.VirtualAddress && rva < dir.VirtualAddress + dir.Size) return nullptr;
    return base + rva;
  }
  return nullptr;
}

}  // namespace

bool is_stub_hooked(const std::uint8_t* code, std::size_t max_scan) noexcept {
  if (!code || max_scan < 5) return false;
  // Clean ntdll stubs always start with mov r10, rcx (4C 8B D1) or mov eax (B8)
  if (code[0] == 0x4C && max_scan >= 3 && code[1] == 0x8B && code[2] == 0xD1)
    return false;
  if (code[0] == 0xB8) return false;

  // jmp rel32 / call rel32
  if (code[0] == 0xE9 || code[0] == 0xE8) return true;
  // jmp/call [rip+disp32]
  if (max_scan >= 6 && code[0] == 0xFF && (code[1] == 0x25 || code[1] == 0x15)) return true;
  // mov rax, imm64; jmp rax
  if (max_scan >= 12 && code[0] == 0x48 && code[1] == 0xB8 &&
      code[10] == 0xFF && code[11] == 0xE0)
    return true;
  // int3 fill
  if (code[0] == 0xCC) return true;
  // push imm; ret
  if (max_scan >= 6 && code[0] == 0x68 && code[5] == 0xC3) return true;
  return true;  // unknown prologue — treat as hooked/suspicious
}

int extract_ssn_from_stub(const std::uint8_t* code, std::size_t max_scan) noexcept {
  if (!code || max_scan < 8) return -1;

  // Windows 10 classic:
  //   4C 8B D1 | B8 SSN | 0F 05 | C3
  // Windows 11 / instrumentation (SharedUserData syscall bit):
  //   4C 8B D1 | B8 SSN | F6 04 25 08 03 FE 7F 01 | 75 03 | 0F 05 | C3 | CD 2E | C3
  // Halo/Tartarus may also find the pattern after a JMP hook at an offset.

  auto has_syscall_nearby = [&](const std::uint8_t* p, std::size_t room) -> bool {
    for (std::size_t i = 0; i + 1 < room; ++i) {
      if (p[i] == 0x0F && p[i + 1] == 0x05) return true;  // syscall
      if (p[i] == 0xCD && p[i + 1] == 0x2E) return true;  // int 2E fallback
    }
    return false;
  };

  for (std::size_t off = 0; off + 8 <= max_scan; ++off) {
    const auto* p = code + off;

    // mov r10, rcx; mov eax, imm32
    if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {
      std::uint32_t ssn = 0;
      std::memcpy(&ssn, p + 4, sizeof(ssn));
      // Accept if a syscall/int2E appears within the remaining scan window,
      // or if this is the start of a known stub layout (SSN < 0x1000).
      const std::size_t room = max_scan - off - 8;
      if (ssn < 0x1000 && (room == 0 || has_syscall_nearby(p + 8, room) || ssn < 0x200)) {
        return static_cast<int>(ssn);
      }
    }

    // mov eax, imm32; mov r10, rcx  (rarer ordering)
    if (p[0] == 0xB8 && off + 8 <= max_scan && p[5] == 0x4C && p[6] == 0x8B &&
        p[7] == 0xD1) {
      std::uint32_t ssn = 0;
      std::memcpy(&ssn, p + 1, sizeof(ssn));
      const std::size_t room = max_scan - off - 8;
      if (ssn < 0x1000 && (room == 0 || has_syscall_nearby(p + 8, room) || ssn < 0x200)) {
        return static_cast<int>(ssn);
      }
    }
  }
  return -1;
}

SsnResult resolve_ssn_robust(const char* nt_name) noexcept {
  SsnResult result{};
  if (!nt_name || !*nt_name) return result;

  uintptr_t ntdll = peb::find_module("ntdll");
  if (!ntdll) return result;

  char alt[128];
  to_zw_name(nt_name, alt, sizeof(alt));

  const std::uint8_t* addr = find_export(ntdll, nt_name);
  if (!addr) addr = find_export(ntdll, alt);
  if (!addr) return result;

  // 1) Hell's Gate — clean stub
  int ssn = extract_ssn_from_stub(addr, 32);
  if (ssn >= 0 && !is_stub_hooked(addr, 16)) {
    result.number = ssn;
    result.method = SsnMethod::HellsGate;
    result.was_hooked = false;
    return result;
  }

  const bool hooked = is_stub_hooked(addr, 16);
  result.was_hooked = hooked;

  // 2) Tartarus' Gate — scan past hook into body (up to 64 bytes)
  ssn = extract_ssn_from_stub(addr, 64);
  if (ssn >= 0) {
    result.number = ssn;
    result.method = SsnMethod::TartarusGate;
    return result;
  }

  // 3) Halo's Gate — neighbor walk on sorted Zw* address list
  static std::vector<ExportEntry> zw_list;
  static bool zw_ready = false;
  if (!zw_ready) {
    zw_ready = collect_zw_exports(ntdll, zw_list);
  }
  if (!zw_ready || zw_list.empty()) return result;

  char zw_target[128];
  to_zw_name(nt_name, zw_target, sizeof(zw_target));

  int target_idx = -1;
  for (int i = 0; i < static_cast<int>(zw_list.size()); ++i) {
    if (name_eq_ci(zw_list[static_cast<std::size_t>(i)].name, zw_target)) {
      target_idx = i;
      break;
    }
  }
  if (target_idx < 0) {
    // Try Nt name against Zw list by suffix
    char nt_form[128];
    to_nt_name(nt_name, nt_form, sizeof(nt_form));
    to_zw_name(nt_form, zw_target, sizeof(zw_target));
    for (int i = 0; i < static_cast<int>(zw_list.size()); ++i) {
      if (name_eq_ci(zw_list[static_cast<std::size_t>(i)].name, zw_target)) {
        target_idx = i;
        break;
      }
    }
  }
  if (target_idx < 0) return result;

  // Walk downward then upward for a clean neighbor
  for (int delta = 1; delta < static_cast<int>(zw_list.size()); ++delta) {
    for (int dir = -1; dir <= 1; dir += 2) {
      int ni = target_idx + dir * delta;
      if (ni < 0 || ni >= static_cast<int>(zw_list.size())) continue;
      const auto& neigh = zw_list[static_cast<std::size_t>(ni)];
      if (is_stub_hooked(neigh.address, 16)) continue;
      int nssn = extract_ssn_from_stub(neigh.address, 32);
      if (nssn < 0) continue;
      // SSN delta equals address-order delta for Zw* stubs
      int recovered = nssn - dir * delta;
      if (recovered < 0) continue;
      result.number = recovered;
      result.method = SsnMethod::HalosGate;
      return result;
    }
  }

  // 4) Sorted-index fallback: count clean SSNs below target and use index
  // as SSN when the table is fully sequential (Windows 10/11 default).
  int clean_below = 0;
  bool sequence_ok = true;
  int last_ssn = -1;
  for (int i = 0; i < target_idx; ++i) {
    const auto& e = zw_list[static_cast<std::size_t>(i)];
    if (is_stub_hooked(e.address, 16)) continue;
    int es = extract_ssn_from_stub(e.address, 32);
    if (es < 0) {
      sequence_ok = false;
      break;
    }
    if (last_ssn >= 0 && es <= last_ssn) {
      sequence_ok = false;
      break;
    }
    last_ssn = es;
    ++clean_below;
  }
  if (sequence_ok && last_ssn >= 0) {
    // Estimate: next SSN after last clean one, plus remaining gap to target
    int gap = target_idx - (target_idx - 1);  // at least 1
    // Better: target_idx itself is often the SSN on modern Windows for Zw*
    result.number = target_idx;
    result.method = SsnMethod::SortedIndex;
    (void)clean_below;
    (void)gap;
    return result;
  }

  return result;
}

int resolve_ssn_number(const char* nt_name) noexcept {
  return resolve_ssn_robust(nt_name).number;
}

}  // namespace real::win

#endif  // LR_PLATFORM_WINDOWS && LR_ARCH_X64
