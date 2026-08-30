#include "blue_example.hpp"
#include <cstdio>

namespace examples::hw_monitor_disguise {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.hw_monitor_disguise_active) {
    result.reasons.emplace_back("HW monitor disguise active");
    result.disguise_detected = true;
  }
  if (w.hw_monitor_decoy_osd_active) result.reasons.emplace_back("decoy OSD display active");
  if (w.hw_monitor_rtss_hijack) result.reasons.emplace_back("RTSS process hijacked");

  for (const auto& [pid, proc] : w.processes) {
    if (proc.looks_reputable && !proc.is_game && !proc.is_ac) {
      for (const auto& h : w.handles_to(w.game_pid())) {
        if (h.owner_pid == pid && sim::has(h.access, sim::AccessMask::VmRead)) {
          result.reasons.emplace_back("reputable-looking process pid=" + std::to_string(pid) + " has VmRead handle");
          break;
        }
      }
    }
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.disguise_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 hw_monitor_disguise] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::hw_monitor_disguise
