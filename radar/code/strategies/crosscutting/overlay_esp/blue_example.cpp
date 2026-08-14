#include "blue_example.hpp"
#include "sim/overlay_detection.hpp"

#include <cstdio>

namespace examples::overlay_esp {

BlueResult detect(sim::World& w) {
  sim::OverlayDetector detector(w);
  const auto report = detector.full_check();
  BlueResult result;

  for (const auto& reason : report.reasons) {
    result.reasons.push_back(reason);
  }
  if (report.steam_report.steam_hook_detected ||
      report.steam_report.present_ptr_overwritten) {
    result.reasons.emplace_back("steam_overlay_present_hook");
  }
  if (report.window_report.swapchain_hijacked || report.window_report.cross_process) {
    result.reasons.emplace_back("swapchain_cross_process_hijack");
  }
  for (const auto& ov : w.overlays) {
    if (ov.steam_overlay_hijacked) {
      result.reasons.emplace_back("steam_overlay_hijacked_window");
      break;
    }
  }
  for (const auto& ov : w.overlays) {
    if (ov.window_hijacked || ov.cross_process_hijack) {
      result.reasons.emplace_back("window_hijack_overlay");
      break;
    }
  }

  bool foreign_vm = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    const auto* p = w.proc(h.owner_pid);
    if (p && !p->is_game && !p->is_ac &&
        sim::has(h.access, sim::AccessMask::VmRead)) {
      foreign_vm = true;
      result.reasons.emplace_back("foreign_vm_read_with_overlay_product");
      break;
    }
  }

  const bool specific_scar =
      report.steam_report.steam_hook_detected ||
      report.steam_report.present_ptr_overwritten ||
      report.window_report.swapchain_hijacked ||
      report.window_report.cross_process ||
      !w.overlays.empty();
  if (specific_scar) {
    result.reasons.emplace_back("strategy scar: overlay ESP hijack product");
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && specific_scar;
  result.mitigated = report.any_mitigation || result.signals >= 3;
  if (result.mitigated) w.ranked_access_denied = true;
  result.detail = report.detail.empty()
                      ? ("overlay_esp blue signals=" + std::to_string(result.signals) +
                         " detected=" + std::to_string(result.detected ? 1 : 0) +
                         " foreign_vm=" + std::to_string(foreign_vm ? 1 : 0))
                      : (report.detail + " signals=" + std::to_string(result.signals));
  w.note(result.detail);
  std::printf("[overlay_esp] BLUE signals=%d detected=%s\n", result.signals,
              result.detected ? "true" : "false");
  for (const auto& reason : result.reasons) {
    std::printf("[overlay_esp]   %s\n", reason.c_str());
  }
  return result;
}

}  // namespace examples::overlay_esp
