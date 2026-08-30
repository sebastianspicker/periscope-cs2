#include "blue_example.hpp"

#include <algorithm>
#include <utility>

namespace examples::ept_violation_evade {
namespace {

void add_signal(BlueResult& result, std::string reason, double weight) {
  ++result.signals;
  result.risk += weight;
  result.reasons.push_back(std::move(reason));
}

}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult result{false, false, 0, 0.0, {}, ""};
  const bool personal_hv = w.trust.personal_hv_active;
  const bool invept_used = w.invept_tlb_flush_used;

  if (w.ept_sidechannel_active) {
    add_signal(result, "EPT side-channel residual is present", 0.55);
  }
  if (invept_used) {
    add_signal(result, "unusual INVEPT TLB flush was recorded", 0.15);
  }
  if (w.ept_ac_scan_evasions > 0) {
    add_signal(result, "AC scan windows were evaded", 0.15);
  }
  if (personal_hv) {
    add_signal(result, "personal HV coordinator is active", 0.10);
  }
  if (!w.trust.vbs || !w.trust.hvci || !w.trust.hvci_enabled) {
    add_signal(result, "VBS/HVCI trust policy is disabled", 0.15);
  }

  result.risk = std::min(result.risk, 1.0);
  result.detected = result.signals >= 2;
  // EPT timing resembles ordinary virtualization overhead without HV introspection.
  result.mitigated = result.risk >= 0.80;
  if (result.mitigated) {
    w.ranked_access_denied = true;
  }
  result.detail = "EPT side-channel detection is HV-introspection limited; signals=" +
                  std::to_string(result.signals) +
                  " risk=" + std::to_string(result.risk);
  w.note(result.detail);
  return result;
}

}  // namespace examples::ept_violation_evade