// trust_policy.cpp — T3 blue platform/HV sensor using World.trust posture flags.
// Simulated personal HV / attestation fields

#include "t3_blue/trust_policy.hpp"

namespace t3_blue {

TrustPolicy::TrustPolicy(ac::ITelemetrySink& sink) : sink_(sink) {}

// TrustPolicy::from_world: Capture HostTrustState from World.trust fields.
HostTrustState TrustPolicy::from_world(const sim::World& w) const {
  HostTrustState h;
  h.secure_boot = w.trust.secure_boot;
  h.vbs = w.trust.vbs;
  h.hvci = w.trust.hvci;
  h.unexpected_hypervisor = w.trust.personal_hv_active &&
                            w.trust.hv_vendor != w.trust.platform_hv_vendor &&
                            !w.trust.hv_vendor.empty();
  return h;
}

// TrustPolicy::evaluate_ranked: TrustPolicy decision for ranked admission from host posture.
PolicyResult TrustPolicy::evaluate_ranked(const HostTrustState& host) {
  if (require_vbs_ && !host.vbs) {
    sink_.emit(ac::TelemetryEvent{
        .kind = ac::EventKind::TrustPolicyFail,
        .related_tier = ac::Tier::T3_Hypervisor,
        .detail = "vbs_off",
        .risk_delta = 6.0,
    });
    return {false, "vbs_required", 6.0};
  }
  if (require_hvci_ && !host.hvci) {
    sink_.emit(ac::TelemetryEvent{
        .kind = ac::EventKind::TrustPolicyFail,
        .related_tier = ac::Tier::T3_Hypervisor,
        .detail = "hvci_off",
        .risk_delta = 6.0,
    });
    return {false, "hvci_required", 6.0};
  }
  if (require_secure_boot_ && !host.secure_boot) {
    sink_.emit(ac::TelemetryEvent{
        .kind = ac::EventKind::TrustPolicyFail,
        .related_tier = ac::Tier::T3_Hypervisor,
        .detail = "secure_boot_off",
        .risk_delta = 4.0,
    });
    return {false, "secure_boot_required", 4.0};
  }
  if (host.unexpected_hypervisor) {
    sink_.emit(ac::TelemetryEvent{
        .kind = ac::EventKind::HvProbeAnomaly,
        .related_tier = ac::Tier::T3_Hypervisor,
        .detail = "unexpected_hv",
        .risk_delta = 5.0,
    });
    return {false, "unexpected_hypervisor", 5.0};
  }
  return {true, "ok", 0};
}

// TrustPolicy::evaluate_world: Map World.trust into HostTrustState then evaluate_ranked.
PolicyResult TrustPolicy::evaluate_world(const sim::World& w) {
  return evaluate_ranked(from_world(w));
}

}  // namespace t3_blue
