#include "blue_example.hpp"

#include <algorithm>
#include <string>

namespace examples::smm_read_channel {
namespace {

void add_signal(BlueResult& result, std::string reason, double weight) {
  ++result.signals;
  result.risk += weight;
  result.reasons.push_back(std::move(reason));
}

}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult result;
  const bool personal_hv = w.trust.personal_hv_active;

  if (w.smm_read_channel_planted) {
    add_signal(result, "SMM read-channel residual is present", 0.50);
  }
  if (w.smm_residual) {
    add_signal(result, "firmware-class SMM residual is present", 0.20);
  }
  if (personal_hv) {
    add_signal(result, "personal HV coordinator is active", 0.15);
  }
  if (!w.trust.vbs || !w.trust.hvci || !w.trust.hvci_enabled) {
    add_signal(result, "VBS/HVCI trust policy is disabled", 0.20);
  }
  if (w.trust.unexpected_efi_entry) {
    add_signal(result, "unexpected EFI boot-level entry is present", 0.15);
  }

  result.risk = std::min(result.risk, 1.0);
  result.detected = result.signals >= 2;
  // The lab has no platform-firmware monitor; high-confidence policy denial
  // is the only post-compromise response represented by this scenario.
  result.mitigated = result.risk >= 0.8;
  if (result.mitigated) {
    w.ranked_access_denied = true;
  }
  result.detail = "SMM detection requires firmware support; signals=" +
                  std::to_string(result.signals) +
                  " risk=" + std::to_string(result.risk);
  w.note(result.detail);
  return result;
}

}  // namespace examples::smm_read_channel