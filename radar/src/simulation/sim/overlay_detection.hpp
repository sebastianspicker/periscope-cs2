#pragma once

// Blue-side overlay hijack detection sensors.
// Based on DXGI OutputWindow ownership and Steam trampoline integrity checks.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace sim {

struct SteamOverlayHookReport {
  bool gameoverlay_loaded = false;
  bool present_ptr_overwritten = false;
  bool resize_buffers_overwritten = false;
  std::uint64_t expected_present_ptr = 0;
  std::uint64_t current_present_ptr = 0;
  std::uint64_t expected_resize_ptr = 0;
  std::uint64_t current_resize_ptr = 0;
  bool steam_hook_detected = false;
  std::string detail;
};

struct WindowHijackReport {
  int swap_chain_count = 0;
  std::uint64_t output_window = 0;
  std::uint32_t hwnd_owner_pid = 0;
  std::uint32_t presenter_pid = 0;
  bool cross_process = false;
  bool swapchain_hijacked = false;
  std::vector<std::string> indicators;
  std::string detail;
};

struct OverlayIntegrityReport {
  SteamOverlayHookReport steam_report;
  WindowHijackReport window_report;
  bool any_detection = false;
  bool any_mitigation = false;
  double risk_score = 0.0;
  std::vector<std::string> reasons;
  std::string detail;
};

class OverlayDetector {
 public:
  explicit OverlayDetector(World& world);

  SteamOverlayHookReport check_steam_overlay();
  WindowHijackReport check_window_hijack();
  OverlayIntegrityReport full_check();
  bool mitigate();

 private:
  World& world_;
};

}  // namespace sim
