#include "blue_example.hpp"
#include "sim/overlay_detection.hpp"

#include <cstdio>

namespace examples::dxgi_present_hook {

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

  sim::OverlayDetector detector(w);
  const auto report = detector.full_check();

  bool present_hooked = false;
  for (const auto& module : game->modules) {
    if (module.present_hooked) {
      present_hooked = true;
      break;
    }
  }

  // Independent sensors — not a single OverlayDetector flag echo.
  if (present_hooked || w.steam_present_hooked || w.vtable_present_hook_fallback) {
    result.reasons.emplace_back("strategy scar: DXGI Present detour on game module");
    result.risk += 0.35;
  }
  if (report.steam_report.steam_hook_detected ||
      report.steam_report.present_ptr_overwritten || w.steam_trampoline_hooks_written) {
    result.reasons.emplace_back("Steam overlay trampoline Present/Resize mismatch");
    result.risk += 0.25;
  }
  if (report.window_report.swapchain_hijacked || report.window_report.cross_process ||
      w.cross_process_hijack || w.window_hijacked) {
    result.reasons.emplace_back("cross-process swapchain/window hijack residual");
    result.risk += 0.20;
  }
  if (w.swapchain_pinned || w.steam_critical_section_active) {
    result.reasons.emplace_back("pinned swapchain or steam critical-section residual");
    result.risk += 0.10;
  }

  int foreign_vm_read = 0;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    const auto* owner = w.proc(handle.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      ++foreign_vm_read;
    }
  }
  if (foreign_vm_read > 0) {
    result.reasons.emplace_back("foreign VM_READ handle count=" +
                                std::to_string(foreign_vm_read));
    result.risk += 0.10;
  }

  const bool specific_scar =
      present_hooked || w.steam_present_hooked || w.vtable_present_hook_fallback;
  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = result.detected && (result.signals >= 3 || result.risk >= 0.65);
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = "dxgi_present_hook signals=" + std::to_string(result.signals) +
                  " risk=" + std::to_string(result.risk);

  std::printf("[T0 dxgi_present_hook] BLUE signals=%d risk=%.2f detected=%s\n",
              result.signals, result.risk, result.detected ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[T0 dxgi_present_hook]   %s\n", reason.c_str());
  }
  return result;
}

}  // namespace examples::dxgi_present_hook
