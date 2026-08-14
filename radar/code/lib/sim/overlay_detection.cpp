#include "sim/overlay_detection.hpp"

namespace sim {

namespace {

std::uint64_t expected_pointer(const World& world, std::uint64_t saved,
                               std::uint32_t offset) {
  return saved != 0 ? saved : world.gameoverlay_base + offset;
}

std::uint64_t current_pointer(std::uint64_t expected, std::uint64_t hook,
                              bool overwritten) {
  return overwritten ? (hook != 0 ? hook : expected + 1) : expected;
}

}  // namespace

OverlayDetector::OverlayDetector(World& world) : world_(world) {}

SteamOverlayHookReport OverlayDetector::check_steam_overlay() {
  SteamOverlayHookReport report;
  report.gameoverlay_loaded = world_.gameoverlay_loaded;
  world_.steam_hook_checked = true;
  if (!report.gameoverlay_loaded) {
    report.detail = "GameOverlayRenderer64.dll not loaded";
    world_.steam_hook_detected = false;
    world_.note("overlay detector steam: " + report.detail);
    return report;
  }

  report.expected_present_ptr = expected_pointer(
      world_, world_.original_steam_present_ptr, world_.steam_present_ptr_offset);
  report.expected_resize_ptr = expected_pointer(
      world_, world_.original_steam_resize_buffers_ptr,
      world_.steam_resize_buffers_offset);
  report.current_present_ptr = current_pointer(
      report.expected_present_ptr, world_.hook_present_fn, world_.steam_present_hooked);
  const bool resize_hooked = world_.steam_resize_buffers_hooked ||
                             world_.steam_resize_hooked;
  report.current_resize_ptr = current_pointer(
      report.expected_resize_ptr, world_.hook_resize_buffers_fn, resize_hooked);
  report.present_ptr_overwritten =
      report.current_present_ptr != report.expected_present_ptr;
  report.resize_buffers_overwritten =
      report.current_resize_ptr != report.expected_resize_ptr;
  report.steam_hook_detected = report.present_ptr_overwritten ||
                               report.resize_buffers_overwritten;
  world_.steam_hook_detected = report.steam_hook_detected;
  report.detail = report.steam_hook_detected
                      ? "Steam overlay trampoline pointer mismatch"
                      : "Steam overlay trampoline pointers verified";
  world_.note("overlay detector steam: " + report.detail);
  return report;
}

WindowHijackReport OverlayDetector::check_window_hijack() {
  WindowHijackReport report;
  report.swap_chain_count = world_.swap_chain_count;
  report.output_window = world_.swap_chain_output_window;
  report.hwnd_owner_pid = world_.output_window_owner_pid != 0
                              ? world_.output_window_owner_pid
                              : world_.hwnd_owner_pid;
  report.presenter_pid = world_.presenter_pid;
  report.cross_process = world_.cross_process_swapchain ||
                         world_.cross_process_hijack;
  if (report.swap_chain_count > 0) {
    report.indicators.emplace_back("DXGI swap chain enumerated");
  }
  if (report.cross_process) {
    report.indicators.emplace_back("cross-process swap chain marked");
  }
  if (report.hwnd_owner_pid != 0 && report.presenter_pid != 0 &&
      report.hwnd_owner_pid != report.presenter_pid) {
    report.indicators.emplace_back("OutputWindow owner PID differs from presenter PID");
    report.swapchain_hijacked = true;
  }
  world_.overlay_hijack_checked = true;
  world_.overlay_hijack_detected = report.swapchain_hijacked;
  report.detail = report.swapchain_hijacked
                      ? "DXGI OutputWindow cross-process hijack detected"
                      : "DXGI OutputWindow ownership verified";
  world_.note("overlay detector window: " + report.detail);
  return report;
}

OverlayIntegrityReport OverlayDetector::full_check() {
  OverlayIntegrityReport report;
  report.steam_report = check_steam_overlay();
  report.window_report = check_window_hijack();
  if (report.steam_report.steam_hook_detected) {
    report.reasons.emplace_back(report.steam_report.detail);
    report.risk_score += 0.5;
  }
  if (report.window_report.swapchain_hijacked) {
    report.reasons.emplace_back(report.window_report.detail);
    report.risk_score += 0.5;
  }
  report.any_detection = !report.reasons.empty();
  report.any_mitigation = report.any_detection && mitigate();
  report.detail = report.any_detection ? "overlay integrity violation" :
                                          "overlay integrity verified";
  world_.note("overlay detector full: " + report.detail);
  return report;
}

bool OverlayDetector::mitigate() {
  if (!world_.steam_hook_detected && !world_.overlay_hijack_detected) {
    return false;
  }
  if (world_.steam_hook_detected && world_.overlay_hijack_detected) {
    world_.ranked_access_denied = true;
  }
  world_.note("overlay detector mitigation: overlay-based ESP blocked");
  return true;
}

}  // namespace sim
