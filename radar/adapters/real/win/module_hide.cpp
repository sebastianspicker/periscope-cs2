#include "real/win/module_hide.hpp"
#include "real/platform.hpp"

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64

#include "real/win/peb_util.hpp"
#include "real/win/windows_h.hpp"

#include <cstring>

namespace real::win {
namespace {

// Full LDR_DATA_TABLE_ENTRY layout needed for three list links.
#pragma pack(push, 8)
struct LdrEntryFull {
  LIST_ENTRY InLoadOrderLinks;
  LIST_ENTRY InMemoryOrderLinks;
  LIST_ENTRY InInitializationOrderLinks;
  void* DllBase;
  void* EntryPoint;
  uint32_t SizeOfImage;
  UNICODE_STRING FullDllName;
  UNICODE_STRING BaseDllName;
};
#pragma pack(pop)

struct SavedLinks {
  LIST_ENTRY load{};
  LIST_ENTRY mem{};
  LIST_ENTRY init{};
  LdrEntryFull* entry = nullptr;
  bool valid = false;
};

static SavedLinks s_saved{};

void unlink_list_entry(LIST_ENTRY* e) {
  if (!e || !e->Flink || !e->Blink) return;
  e->Blink->Flink = e->Flink;
  e->Flink->Blink = e->Blink;
  // Self-loop so double-unlink is safe
  e->Flink = e;
  e->Blink = e;
}

void relink_list_entry(LIST_ENTRY* e, LIST_ENTRY* saved) {
  if (!e || !saved || !saved->Flink || !saved->Blink) return;
  // Insert between saved Blink and Flink
  e->Blink = saved->Blink;
  e->Flink = saved->Flink;
  saved->Blink->Flink = e;
  saved->Flink->Blink = e;
}

LdrEntryFull* find_entry(void* base) {
  auto* peb = peb::get_peb();
  if (!peb || !peb->Ldr) return nullptr;
  auto* head = &peb->Ldr->InLoadOrderModuleList;
  for (auto* cur = head->Flink; cur != head; cur = cur->Flink) {
    auto* e = reinterpret_cast<LdrEntryFull*>(cur);
    if (e->DllBase == base) return e;
  }
  return nullptr;
}

}  // namespace

ModuleHideReport module_unlink(std::uint64_t module_base) noexcept {
  ModuleHideReport rep{};
  if (s_saved.valid) {
    rep.detail = "already unlinked";
    rep.unlinked = true;
    rep.module_base = s_saved.entry ? reinterpret_cast<std::uint64_t>(s_saved.entry->DllBase) : 0;
    return rep;
  }

  auto* peb = peb::get_peb();
  if (!peb) {
    rep.detail = "no PEB";
    return rep;
  }
  void* base = module_base ? reinterpret_cast<void*>(static_cast<uintptr_t>(module_base))
                           : peb->ImageBaseAddress;
  rep.module_base = reinterpret_cast<std::uint64_t>(base);

  LdrEntryFull* entry = find_entry(base);
  if (!entry) {
    rep.detail = "LDR entry not found";
    return rep;
  }

  // Save links before unlink
  s_saved.load = entry->InLoadOrderLinks;
  s_saved.mem = entry->InMemoryOrderLinks;
  s_saved.init = entry->InInitializationOrderLinks;
  s_saved.entry = entry;
  s_saved.valid = true;

  unlink_list_entry(&entry->InLoadOrderLinks);
  unlink_list_entry(&entry->InMemoryOrderLinks);
  unlink_list_entry(&entry->InInitializationOrderLinks);

  rep.unlinked = true;
  rep.detail = "unlinked";
  return rep;
}

ModuleHideReport module_relink() noexcept {
  ModuleHideReport rep{};
  if (!s_saved.valid || !s_saved.entry) {
    rep.detail = "nothing to relink";
    return rep;
  }
  rep.module_base = reinterpret_cast<std::uint64_t>(s_saved.entry->DllBase);

  // Restore saved neighbor pointers then reinsert
  // Use copies of saved links (unlink mutated the entry's LIST_ENTRY)
  LIST_ENTRY load = s_saved.load;
  LIST_ENTRY mem = s_saved.mem;
  LIST_ENTRY init = s_saved.init;

  relink_list_entry(&s_saved.entry->InLoadOrderLinks, &load);
  relink_list_entry(&s_saved.entry->InMemoryOrderLinks, &mem);
  relink_list_entry(&s_saved.entry->InInitializationOrderLinks, &init);

  s_saved = {};
  rep.relinked = true;
  rep.detail = "relinked";
  return rep;
}

bool module_is_hidden() noexcept { return s_saved.valid; }

}  // namespace real::win

#else

namespace real::win {
ModuleHideReport module_unlink(std::uint64_t) noexcept { return {}; }
ModuleHideReport module_relink() noexcept { return {}; }
bool module_is_hidden() noexcept { return false; }
}  // namespace real::win

#endif
