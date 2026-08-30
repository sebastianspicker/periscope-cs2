#include "red_example.hpp"
#include "t0_red/cheat_client.hpp"

#include <cstdio>

namespace examples::dxgi_present_hook {

RedResult apply(sim::World& w) {
  int steps = 0;
  t0_red::CheatClient cheat(w, "dxgi-hook.exe");

  auto* game = w.proc(w.game_pid());
  if (!game || !game->is_game) {
    return {false, steps, "no game"};
  }
  std::printf("[T0 dxgi_present_hook] step %d: game pid=%u validated\n", ++steps,
              w.game_pid());

  // Phase 1: prefer Steam overlay Present trampoline hijack.
  const bool steam_ok = cheat.enable_steam_overlay_hijack();
  if (steam_ok) {
    std::printf("[T0 dxgi_present_hook] step %d: steam overlay Present hijack\n",
                ++steps);
  } else {
    // Phase 1b: vtable Present fallback when overlay path unavailable.
    cheat.enable_vtable_present_fallback();
    std::printf("[T0 dxgi_present_hook] step %d: vtable Present fallback\n",
                ++steps);
  }

  // Phase 2: cross-process window/swapchain companion surface.
  cheat.enable_window_hijack_overlay("Discord.exe");
  std::printf("[T0 dxgi_present_hook] step %d: window hijack overlay companion\n",
              ++steps);

  // Phase 3: pin swapchain + critical section residual (second scar family).
  cheat.enable_pinned_swapchain(120);
  cheat.enable_steam_critical_section();
  std::printf("[T0 dxgi_present_hook] step %d: pinned swapchain + critical section\n",
              ++steps);

  const bool scar =
      (w.steam_present_hooked || w.vtable_present_hook_fallback) &&
      (w.window_hijacked || w.cross_process_hijack) && w.swapchain_pinned;
  if (!scar) {
    return {false, steps, "dxgi present multi-scar verification failed"};
  }

  const std::string detail =
      "steam_overlay_hijack + window_hijack + pinned_swapchain";
  w.note("dxgi_present_hook: " + detail);
  return {true, steps, detail};
}

}  // namespace examples::dxgi_present_hook
