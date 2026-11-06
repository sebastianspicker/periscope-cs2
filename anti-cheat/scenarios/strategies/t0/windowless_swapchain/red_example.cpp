#include "red_example.hpp"

#include <cstdio>

namespace examples::windowless_swapchain {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 windowless_swapchain] FAIL: game process is unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T0 windowless_swapchain] step %d: game pid=%u validated\n",
              ++steps, game_pid);

  const auto patcher = w.spawn("present-patcher.exe");
  if (!w.proc(patcher)) {
    std::printf("[T0 windowless_swapchain] FAIL: patcher creation was not observable\n");
    return {false, steps, "present patcher spawn verification failed"};
  }
  std::printf("[T0 windowless_swapchain] step %d: present patcher pid=%u created\n",
              ++steps, patcher);

  const auto patch_access = sim::AccessMask::VmRead | sim::AccessMask::VmWrite;
  if (!w.open_process(patcher, game_pid, patch_access, false)) {
    std::printf("[T0 windowless_swapchain] FAIL: VM_READ/VM_WRITE handle denied\n");
    return {false, steps, "OpenProcess VM_READ/VM_WRITE failed"};
  }
  bool has_patch_handle = false;
  for (const auto& handle : w.handles_to(game_pid)) {
    if (handle.owner_pid == patcher &&
        sim::has(handle.access, sim::AccessMask::VmRead) &&
        sim::has(handle.access, sim::AccessMask::VmWrite)) {
      has_patch_handle = true;
      break;
    }
  }
  if (!has_patch_handle) {
    return {false, steps, "VM_READ/VM_WRITE handle verification failed"};
  }
  std::printf("[T0 windowless_swapchain] step %d: patch handle verified\n", ++steps);

  auto read = w.read_mem(patcher, game_pid, game->base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    std::printf("[T0 windowless_swapchain] FAIL: entity memory read failed\n");
    return {false, steps, "entity memory read failed"};
  }
  std::printf("[T0 windowless_swapchain] step %d: entity memory read recorded\n", ++steps);

  sim::Module* present_module = nullptr;
  for (auto& module : game->modules) {
    if (module.name == "client.dll" || module.name == "game.exe") {
      present_module = &module;
      break;
    }
  }
  if (!present_module) {
    return {false, steps, "no game module available for Present hook"};
  }
  present_module->present_hooked = true;
  w.windowless_swapchain_hijack = true;
  w.swapchain_hijacked_no_window = true;
  w.add_overlay({patcher, "swapchain-internal-marker", false, false, true});

  bool windowless_marker = false;
  bool visible_overlay = false;
  for (const auto& overlay : w.overlays) {
    if (overlay.owner_pid == patcher && overlay.hijacks_swapchain &&
        !overlay.topmost && !overlay.transparent) {
      windowless_marker = true;
    }
    visible_overlay = visible_overlay || overlay.topmost || overlay.transparent;
  }
  const bool hijacked_without_visible_window = present_module->present_hooked &&
      w.windowless_swapchain_hijack && w.swapchain_hijacked_no_window &&
      windowless_marker && !visible_overlay;
  if (!hijacked_without_visible_window) {
    std::printf("[T0 windowless_swapchain] FAIL: windowless swapchain scar verification failed\n");
    return {false, steps, "windowless swapchain scar verification failed"};
  }
  std::printf("[T0 windowless_swapchain] step %d: windowless Present hook verified\n",
              ++steps);
  const std::string detail =
      "windowless_swapchain: Present hook with no visible overlay window";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::windowless_swapchain
