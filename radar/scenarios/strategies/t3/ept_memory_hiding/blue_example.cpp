#include "blue_example.hpp"
#include <cstdio>

namespace examples::ept_memory_hiding {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.ept_memory_hiding_active) {
    result.reasons.emplace_back("EPT memory hiding active");
    result.ept_hiding_detected = true;
  }
  if (w.ept_hidden_page_count > 0) result.reasons.emplace_back("EPT hidden pages=" + std::to_string(w.ept_hidden_page_count));
  if (w.ept_invept_used) result.reasons.emplace_back("INVEPT TLB flush used");
  if (w.trust.ept_hide_ac_pages) result.reasons.emplace_back("EPT hides AC pages from guest");
  if (w.ept_sidechannel_active) result.reasons.emplace_back("EPT side-channel evasion active");
  if (w.invept_tlb_flush_used) result.reasons.emplace_back("INVEPT TLB flush for AC scan evasion");

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.ept_hiding_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T3 ept_memory_hiding] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::ept_memory_hiding
