#include "blue_example.hpp"
#include <cstdio>

namespace examples::helper_ticket_protocol {

BlueResult detect(sim::World& w) {
  BlueResult result;
  for (const auto& sec : w.sections) {
    if (sec.carries_entity_bytes && !sec.name.empty()) {
      result.reasons.emplace_back("shared section carries entity bytes: " + sec.name);
      result.shared_section_found = true;
    }
  }
  if (w.helper_ticket_active) {
    result.reasons.emplace_back("helper ticket protocol active");
    result.shared_section_found = true;
  }
  if (w.helper_ticket_shared_memory_created) result.reasons.emplace_back("helper ticket shared memory created");
  if (w.helper_ticket_claimed) result.reasons.emplace_back("helper ticket claimed by consumer");

  result.signals = static_cast<int>(result.reasons.size());
  result.detected = result.signals >= 2 && result.shared_section_found;
  if (result.detected) result.mitigated = true;
  for (const auto& r : result.reasons) std::printf("[T0 helper_ticket_protocol] BLUE: %s\n", r.c_str());
  return result;
}

}  // namespace examples::helper_ticket_protocol
