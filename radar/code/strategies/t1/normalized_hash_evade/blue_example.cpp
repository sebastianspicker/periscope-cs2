#include "blue_example.hpp"
#include <cstdio>

namespace examples::normalized_hash_evade {

BlueResult detect(sim::World& w) {
  BlueResult result;
  if (w.normalized_hash_evade_active) {
    result.reasons.emplace_back("normalized hash evasion active");
    result.hash_normalization_detected = true;
  }
  if (w.pe_hash_normalized) result.reasons.emplace_back("PE hash normalized");
  if (w.pe_section_entries_altered > 0) result.reasons.emplace_back("PE sections altered=" + std::to_string(w.pe_section_entries_altered));

  for (const auto& [pid, proc] : w.processes) {
    for (const auto& m : proc.modules) {
      if (m.text_hash == "normalized") result.reasons.emplace_back("normalized hash in " + m.name + " pid=" + std::to_string(pid));
    }
  }

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.hash_normalization_detected;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T1 normalized_hash_evade] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::normalized_hash_evade
