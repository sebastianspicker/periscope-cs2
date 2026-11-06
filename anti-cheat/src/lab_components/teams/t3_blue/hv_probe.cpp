// hv_probe.cpp — T3 blue platform/HV sensor using World.trust posture flags.
// Simulated personal HV / attestation fields

#include "t3_blue/hv_probe.hpp"

#include <cstring>
#include <sstream>

namespace t3_blue {

HvProbe::HvProbe(ac::ITelemetrySink& sink) : sink_(sink) {}

// HvProbe::analyze: Analyze host/HV signals into a probe result.
void HvProbe::analyze(const HvProbeSample& sample) {
  bool anomaly = false;
  if (sample.cpuid_hypervisor_bit) {
    anomaly = true;
  }
  if (sample.baseline_latency_ns > 0 &&
      sample.cpuid_latency_ns > sample.baseline_latency_ns * 5.0) {
    anomaly = true;
  }
  if (sample.vendor[0] != '\0' &&
      std::strcmp(sample.vendor, "Microsoft Hv") != 0 &&
      std::strcmp(sample.vendor, "KVMKVMKVM") != 0) {
    anomaly = true;
  }
  last_anomaly_ = anomaly;
  if (!anomaly) {
    return;
  }
  sink_.emit(ac::TelemetryEvent{
      .kind = ac::EventKind::HvProbeAnomaly,
      .related_tier = ac::Tier::T3_Hypervisor,
      .detail = sample.vendor,
      .risk_delta = 4.0,
  });
}

// HvProbe::analyze_world: Analyze World.trust HV fields.
HvProbeResult HvProbe::analyze_world(const sim::World& w) {
  HvProbeResult r;
  const auto& t = w.trust;

  HvProbeSample s;
  s.cpuid_hypervisor_bit = t.personal_hv_active;
  s.cpuid_latency_ns = t.cpuid_latency_ns;
  s.baseline_latency_ns = t.baseline_latency_ns;
  std::snprintf(s.vendor, sizeof(s.vendor), "%s", t.hv_vendor.c_str());
  analyze(s);

  r.personal_hv = t.personal_hv_active;
  r.timing_spoof = t.timing_spoofed;
  r.latency_hit = t.baseline_latency_ns > 0 &&
                  t.cpuid_latency_ns > t.baseline_latency_ns * 5.0;
  r.vendor_hit = !t.hv_vendor.empty() && t.hv_vendor != "Microsoft Hv" &&
                 t.hv_vendor != "KVMKVMKVM";

  if (r.personal_hv) {
    r.reasons.push_back("personal_hv vendor=" + t.hv_vendor);
    r.risk += 4.0;
  }
  if (r.timing_spoof) {
    r.reasons.push_back("timing_spoofed");
    r.risk += 2.0;
  }
  if (r.latency_hit) {
    r.reasons.push_back("cpuid_latency_anomaly");
    r.risk += 2.0;
  }
  if (r.vendor_hit && r.personal_hv) {
    r.reasons.push_back("untrusted_hv_vendor");
    r.risk += 1.0;
  }
  if (t.ept_hide_ac_pages) {
    r.reasons.push_back("ept_hide_ac_pages");
    r.risk += 2.5;
  }

  r.anomaly = last_anomaly_ || r.personal_hv || r.timing_spoof || r.latency_hit;
  std::ostringstream oss;
  oss << "hv_probe anomaly=" << (r.anomaly ? 1 : 0)
      << " personal=" << (r.personal_hv ? 1 : 0)
      << " timing=" << (r.timing_spoof ? 1 : 0)
      << " lat=" << (r.latency_hit ? 1 : 0) << " risk=" << r.risk;
  r.detail = oss.str();
  return r;
}

}  // namespace t3_blue
