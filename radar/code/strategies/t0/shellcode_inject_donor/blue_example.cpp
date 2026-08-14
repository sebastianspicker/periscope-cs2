#include "blue_example.hpp"
#include <cstdio>

namespace examples::shellcode_inject_donor {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.shellcode_donor_active) {
    result.reasons.emplace_back("shellcode donor active");
    result.donor_injection_detected = true;
  }
  if (w.shellcode_obfuscated) result.reasons.emplace_back("shellcode obfuscated");
  if (w.shellcode_xor_key_applied) result.reasons.emplace_back("XOR obfuscation key applied");

  for (const auto& [pid, proc] : w.processes) {
    if (proc.has_foreign_thread) result.reasons.emplace_back("foreign thread in pid=" + std::to_string(pid));
    if (proc.manual_mapped_region) result.reasons.emplace_back("manual mapped region in pid=" + std::to_string(pid));
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.donor_injection_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 shellcode_inject_donor] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::shellcode_inject_donor
