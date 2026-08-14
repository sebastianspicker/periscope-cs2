#include "blue_example.hpp"
#include <cstdio>

namespace examples::wda_exclude_capture {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.overlays.empty()) {
    result.reasons.emplace_back("no overlays present");
    result.signals = 1;
    return result;
  }

  int stream_proof_count = 0;
  int foreign_overlays = 0;
  for (const auto& o : w.overlays) {
    const auto* owner = w.proc(o.owner_pid);
    if (owner && !owner->is_game && !owner->is_ac) {
      ++foreign_overlays;
      if (o.stream_proof) ++stream_proof_count;
    }
  }

  if (foreign_overlays > 0) result.reasons.emplace_back("foreign overlay count=" + std::to_string(foreign_overlays));
  if (stream_proof_count > 0) result.reasons.emplace_back("stream-proof overlays=" + std::to_string(stream_proof_count));
  if (w.wda_exclude_updated) result.reasons.emplace_back("WDA exclude attribute applied");
  if (w.wda_exclude_count > 0) result.reasons.emplace_back("WDA reapply count=" + std::to_string(w.wda_exclude_count));

  result.stream_proof_overlay = stream_proof_count > 0;
  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && stream_proof_count > 0;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 wda_exclude_capture] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::wda_exclude_capture
