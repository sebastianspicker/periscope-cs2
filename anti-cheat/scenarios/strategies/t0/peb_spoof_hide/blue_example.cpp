#include "blue_example.hpp"
#include <cstdio>

namespace examples::peb_spoof_hide {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.peb_spoof_active) {
    result.reasons.emplace_back("PEB spoof active");
    result.peb_spoof_detected = true;
  }
  if (w.peb_being_debugged_cleared) result.reasons.emplace_back("BeingDebugged flag cleared");
  if (w.peb_nt_global_flag_cleared) result.reasons.emplace_back("NtGlobalFlag cleared");
  if (w.peb_being_debugged_spoofed) result.reasons.emplace_back("PEB BeingDebugged spoofed");

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.peb_spoof_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 peb_spoof_hide] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::peb_spoof_hide
