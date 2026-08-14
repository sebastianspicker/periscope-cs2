#include "blue_example.hpp"
#include <cstdio>

namespace examples::pattern_scan_offsets {

BlueResult detect(sim::World& w) {
  BlueResult result;
  bool foreign_vm = false;
  for (const auto& h : w.handles_to(w.game_pid())) {
    const auto* owner = w.proc(h.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac && sim::has(h.access, sim::AccessMask::VmRead)) {
      foreign_vm = true;
      break;
    }
  }
  if (foreign_vm) result.reasons.emplace_back("foreign VM_READ handle");
  if (w.remote_read_ops >= 3) {
    result.reasons.emplace_back("bulk remote reads=" + std::to_string(w.remote_read_ops));
    result.bulk_read_detected = true;
  }
  if (w.lab_pattern_marker_present) {
    result.reasons.emplace_back("pattern marker present");
    result.pattern_marker_found = true;
  }
  if (w.lab_pattern_generation > 0) result.reasons.emplace_back("pattern generation=" + std::to_string(w.lab_pattern_generation));

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && (result.bulk_read_detected || result.pattern_marker_found);
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 pattern_scan_offsets] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::pattern_scan_offsets
