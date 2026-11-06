#include "blue_example.hpp"
#include <cstdio>

namespace examples::page_oscillation_mem {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.page_oscillation_active) {
    result.reasons.emplace_back("page oscillation active");
    result.oscillation_detected = true;
  }
  if (w.page_oscillation_cycles > 0) result.reasons.emplace_back("oscillation cycles=" + std::to_string(w.page_oscillation_cycles));
  if (w.code_page_flipped_to_rw) result.reasons.emplace_back("code page flipped to RW");
  if (w.code_page_flipped_to_rx) result.reasons.emplace_back("code page flipped to RX");

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.oscillation_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T1 page_oscillation_mem] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::page_oscillation_mem
