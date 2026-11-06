#include "blue_example.hpp"

#include <cstdio>

namespace examples::windowless_swapchain {

BlueResult detect(sim::World& w) {
  BlueResult result;
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    result.reasons.emplace_back("sensor precondition failed: game unavailable");
    result.signals = static_cast<int>(result.reasons.size());
    result.detail = result.reasons.front();
    return result;
  }

  bool present_hooked = false;
  for (const auto& module : game->modules) {
    present_hooked = present_hooked || module.present_hooked;
  }
  if (present_hooked) {
    result.reasons.emplace_back("game module has a Present hook");
    result.risk += 0.40;
  }
  if (w.windowless_swapchain_hijack) {
    result.reasons.emplace_back("windowless swapchain hijack scar set");
    result.risk += 0.25;
  }

  int foreign_vm_read_handles = 0;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      ++foreign_vm_read_handles;
    }
  }
  if (foreign_vm_read_handles > 0) {
    result.reasons.emplace_back("external VM_READ handle count=" +
                                std::to_string(foreign_vm_read_handles));
    result.risk += 0.15;
  }

  bool windowless_overlay_signature = false;
  for (const auto& overlay : w.overlays) {
    if (overlay.hijacks_swapchain && !overlay.topmost && !overlay.transparent) {
      windowless_overlay_signature = true;
      break;
    }
  }
  if (windowless_overlay_signature) {
    result.reasons.emplace_back("swapchain marker lacks topmost/transparent overlay traits");
    result.risk += 0.20;
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = (present_hooked || w.windowless_swapchain_hijack) &&
                    result.signals >= 2;
  result.mitigated = result.risk >= 0.65;
  result.detail = "windowless swapchain signals=" +
                  std::to_string(result.signals) + " risk=" +
                  std::to_string(result.risk);
  if (result.mitigated) w.ranked_access_denied = true;
  std::printf("[T0 windowless_swapchain] BLUE signals=%d risk=%.2f detected=%s mitigated=%s\n",
              result.signals, result.risk, result.detected ? "true" : "false",
              result.mitigated ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T0 windowless_swapchain]   %s\n", reason.c_str());
  }
  return result;
}

}  // namespace examples::windowless_swapchain
