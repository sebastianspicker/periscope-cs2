#include "blue_example.hpp"
#include <cstdio>

namespace examples::dxgi_output_duplication {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.desktop_duplication) result.reasons.emplace_back("desktop duplication active");
  if (w.dxgi_output_dup_active) result.reasons.emplace_back("DXGI output duplication active");
  if (w.dxgi_output_frames_acquired > 0) result.reasons.emplace_back("DXGI frames acquired=" + std::to_string(w.dxgi_output_frames_acquired));
  if (w.dxgi_output_composite_rendered) result.reasons.emplace_back("DXGI output composite rendered");

  result.output_dup_detected = w.dxgi_output_dup_active || w.desktop_duplication;
  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.output_dup_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 dxgi_output_duplication] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::dxgi_output_duplication
