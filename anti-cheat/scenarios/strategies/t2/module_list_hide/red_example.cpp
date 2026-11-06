#include "red_example.hpp"

#include <cstdio>

namespace strategy::t2_module_list_hide {

void Red::apply(sim::World& w) noexcept {
  std::printf("[red:module_list_hide] step 1: validate game arena\n");
  const auto game = w.game_pid();
  auto* g = w.proc(game);
  if (game == 0 || g == nullptr) {
    w.note("module_list_hide: precondition failed — game missing");
    return;
  }

  const int modules_before = static_cast<int>(g->modules.size());

  std::printf("[red:module_list_hide] step 2: spawn injector actor\n");
  const auto actor = w.spawn("module_list_hide-injector.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    w.note("module_list_hide: actor spawn failed");
    return;
  }

  std::printf("[red:module_list_hide] step 3: map cheat module then unlink from PEB Ldr\n");
  sim::Module mapped{"hidden-cheat.dll", 0x71000000ull, 0x4000, false, true, "foreign"};
  if (!w.inject_module(game, mapped, true)) {
    w.note("module_list_hide: inject_module failed");
    return;
  }
  g = w.proc(game);
  if (g == nullptr) return;

  std::printf("[red:module_list_hide] step 4: plant module-shadow residual flags\n");
  w.module_shadowing_active = true;
  w.module_loaded = true;
  w.module_is_signed = false;
  g->manual_mapped_region = true;
  g->has_foreign_thread = true;

  std::printf("[red:module_list_hide] step 5: verify PEB-unlink vs mapped residual\n");
  int unlinked = 0;
  for (const auto& m : g->modules) {
    if (!m.linked_in_peb && (m.name == "hidden-cheat.dll" || m.text_hash == "foreign")) {
      ++unlinked;
    }
  }
  const int modules_after = static_cast<int>(g->modules.size());
  if (!(w.module_shadowing_active && w.module_loaded && unlinked >= 1 &&
        modules_after >= modules_before)) {
    w.note("module_list_hide: post-condition failed");
    return;
  }

  w.note("module_list_hide: PEB Ldr unlink active; PE headers remain mapped");
  std::printf("[red:module_list_hide] achieved: before=%d after=%d unlinked=%d shadow=1\n",
              modules_before, modules_after, unlinked);
}

}  // namespace strategy::t2_module_list_hide
