// platform_ac.cpp — T3 blue platform/HV sensor using World.trust posture flags.
// Simulated personal HV / attestation fields

#include "t3_blue/platform_ac.hpp"

#include <sstream>
#include <string>

namespace t3_blue {

// Educational multi-sensor platform AC on HostTrust + bridge scars.

PlatformAc::PlatformAc(sim::World& world, ac::ITelemetrySink& sink)
    : world_(world), sink_(sink) {}

// PlatformAc::require_vbs_hvci: Require VBS+HVCI posture for ranked admission.
void PlatformAc::require_vbs_hvci(bool on) { require_trust_ = on; }

// PlatformAc::evaluate_ranked: TrustPolicy decision for ranked admission from host posture.
PlatformDetection PlatformAc::evaluate_ranked() {
  PlatformDetection d;
  if (!require_trust_) {
    d.ranked_allowed = true;
    return d;
  }
  if (!world_.trust.vbs || !world_.trust.hvci) {
    d.ranked_allowed = false;
    d.policy_fail = true;
    d.reasons.push_back("ranked requires VBS+HVCI");
    ac::TelemetryEvent ev{.kind = ac::EventKind::TrustPolicyFail,
                          .related_tier = ac::Tier::T3_Hypervisor,
                          .detail = "vbs_hvci_off",
                          .risk_delta = 6.0};
    sink_.emit(ev);
    risk_.ingest(ev);
  }
  if (world_.trust.personal_hv_active) {
    d.ranked_allowed = false;
    d.hv_anomaly = true;
    d.reasons.push_back("personal HV active: " + world_.trust.hv_vendor);
  }
  if (!world_.trust.attestation_valid || !world_.trust.attestation_pcr_ok) {
    d.attest_fail = true;
    d.ranked_allowed = false;
    d.reasons.push_back("attestation_fail");
  }
  d.risk = risk_.state().score;
  return d;
}

// PlatformAc::probe_hv: Probe unexpected HV / personal HV signals.
PlatformDetection PlatformAc::probe_hv() {
  PlatformDetection d;
  const auto& t = world_.trust;
  if (t.personal_hv_active ||
      (t.baseline_latency_ns > 0 &&
       t.cpuid_latency_ns > t.baseline_latency_ns * 5)) {
    d.hv_anomaly = true;
    d.reasons.push_back("HV probe anomaly vendor=" + t.hv_vendor);
    ac::TelemetryEvent ev{.kind = ac::EventKind::HvProbeAnomaly,
                          .related_tier = ac::Tier::T3_Hypervisor,
                          .detail = t.hv_vendor,
                          .risk_delta = 4.0};
    sink_.emit(ev);
    risk_.ingest(ev);
  }
  if (t.ept_hide_ac_pages || t.secure_kernel_view_dirty) {
    d.ept_dual_view = t.ept_hide_ac_pages;
    d.sk_dirty = t.secure_kernel_view_dirty;
    d.reasons.push_back("ept_or_sk_dual_view");
  }
  if (t.timing_spoofed) {
    d.reasons.push_back("timing_spoofed");
    d.hv_anomaly = true;
  }
  d.risk = risk_.state().score;
  return d;
}

// PlatformAc::scan_bridge: Scan World for bridge residuals.
PlatformDetection PlatformAc::scan_bridge() {
  PlatformDetection d;
  for (const auto& dr : world_.drivers) {
    if (dr.is_bridge) {
      d.bridge_hit = true;
      d.reasons.push_back("bridge driver " + dr.name);
      ac::TelemetryEvent ev{.kind = ac::EventKind::BridgeSuspected,
                            .related_tier = ac::Tier::T3_Hypervisor,
                            .detail = dr.name,
                            .risk_delta = 3.0};
      sink_.emit(ev);
      risk_.ingest(ev);
    }
  }
  for (const auto& dev : world_.devices) {
    if (dev.name.find("HvComm") != std::string::npos ||
        dev.owner_driver.find("hvcomm") != std::string::npos) {
      d.bridge_hit = true;
      d.reasons.push_back("bridge device " + dev.name);
    }
  }
  d.risk = risk_.state().score;
  return d;
}

// PlatformAc::full: Run full platform detection bundle once.
PlatformDetection PlatformAc::full() {
  auto a = evaluate_ranked();
  auto b = probe_hv();
  auto c = scan_bridge();
  PlatformDetection d;
  d.ranked_allowed = a.ranked_allowed;
  d.policy_fail = a.policy_fail;
  d.hv_anomaly = a.hv_anomaly || b.hv_anomaly;
  d.bridge_hit = c.bridge_hit;
  d.attest_fail = a.attest_fail;
  d.ept_dual_view = b.ept_dual_view;
  d.sk_dirty = b.sk_dirty;
  d.trust_aggregate_block =
      d.policy_fail || d.hv_anomaly || d.bridge_hit || d.attest_fail ||
      d.ept_dual_view || d.sk_dirty;
  d.risk = risk_.state().score;
  d.reasons = a.reasons;
  d.reasons.insert(d.reasons.end(), b.reasons.begin(), b.reasons.end());
  d.reasons.insert(d.reasons.end(), c.reasons.begin(), c.reasons.end());
  std::ostringstream oss;
  oss << "platform risk=" << d.risk << " reasons=" << d.reasons.size()
      << " block=" << (d.trust_aggregate_block ? 1 : 0);
  d.detail = oss.str();
  return d;
}

}  // namespace t3_blue
