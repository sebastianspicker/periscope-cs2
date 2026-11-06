// trust_aggregator.cpp — depth analytics: multi-sample / fusion scoring for blue.
// Educational scorers over World residuals.

#include "depth/trust_aggregator.hpp"

#include <sstream>

namespace depth {
namespace {

// Semantic equality of trust-relevant fields (tick ignored).
bool same_trust_payload(const TrustSample& a, const TrustSample& b) {
  return a.secure_boot == b.secure_boot && a.vbs == b.vbs && a.hvci == b.hvci &&
         a.personal_hv == b.personal_hv && a.platform_hv == b.platform_hv &&
         a.timing_spoofed == b.timing_spoofed &&
         a.ept_hide_ac == b.ept_hide_ac &&
         a.attestation_valid == b.attestation_valid &&
         a.vtl1_enclave_present == b.vtl1_enclave_present &&
         a.secure_kernel_view_dirty == b.secure_kernel_view_dirty &&
         a.hvci_race == b.hvci_race && a.smm_residual == b.smm_residual &&
         a.cpuid_latency_ns == b.cpuid_latency_ns &&
         a.baseline_latency_ns == b.baseline_latency_ns &&
         a.hv_vendor == b.hv_vendor;
}

}  // namespace

// TrustAggregator::capture: Capture a trust timeline sample from World.
TrustSample TrustAggregator::capture(const sim::World& w, int tick) const {
  TrustSample s;
  s.tick = tick;
  s.secure_boot = w.trust.secure_boot;
  s.vbs = w.trust.vbs;
  s.hvci = w.trust.hvci;
  s.personal_hv = w.trust.personal_hv_active;
  s.platform_hv = w.trust.platform_hv_active;
  s.timing_spoofed = w.trust.timing_spoofed;
  s.ept_hide_ac = w.trust.ept_hide_ac_pages;
  s.attestation_valid = w.trust.attestation_valid;
  s.vtl1_enclave_present = w.vtl1_enclave_present;
  s.secure_kernel_view_dirty = w.trust.secure_kernel_view_dirty;
  s.hvci_race = !w.trust.hvci_enabled && w.trust.hvci;  // claimed vs enforced
  s.smm_residual = w.smm_residual;
  s.cpuid_latency_ns = w.trust.cpuid_latency_ns;
  s.baseline_latency_ns = w.trust.baseline_latency_ns;
  s.hv_vendor = w.trust.hv_vendor;
  return s;
}

// TrustAggregator::push: Push a HostTrustState sample into the aggregator.
void TrustAggregator::push(const TrustSample& s) { samples_.push_back(s); }

// TrustAggregator::evaluate: Syscall-aware monitor: correlate handles with syscall/soft signals.
TrustAggregateResult TrustAggregator::evaluate() const {
  TrustAggregateResult r;
  r.samples = static_cast<int>(samples_.size());

  // Union of distinct signal families across the timeline (NOT per-tick sum).
  // Frozen World re-sampled N times must not scale risk ×N.
  bool vbs_off = false;
  bool hvci_off = false;
  bool secure_boot_off = false;
  bool attest_fail = false;
  bool personal_hv = false;
  bool timing_spoof = false;
  bool ept_hide = false;
  bool vtl1_miss = false;
  bool sk_dirty = false;
  bool smm = false;
  bool cpuid_lat = false;
  bool vbs_vs_hv = false;
  bool timing_and_hv = false;

  for (const auto& s : samples_) {
    if (require_vbs_ && !s.vbs) {
      vbs_off = true;
    }
    if (require_hvci_ && !s.hvci) {
      hvci_off = true;
    }
    if (require_secure_boot_ && !s.secure_boot) {
      secure_boot_off = true;
    }
    if (!s.attestation_valid) {
      attest_fail = true;
    }
    if (require_no_smm_ && s.smm_residual) {
      smm = true;
    }
    if (s.personal_hv) {
      personal_hv = true;
    }
    if (s.timing_spoofed) {
      timing_spoof = true;
    }
    if (s.ept_hide_ac) {
      ept_hide = true;
    }
    if (!s.vtl1_enclave_present) {
      vtl1_miss = true;
    }
    if (s.secure_kernel_view_dirty) {
      sk_dirty = true;
    }
    if (s.vbs && s.personal_hv) {
      vbs_vs_hv = true;
    }
    if (s.baseline_latency_ns > 0 &&
        s.cpuid_latency_ns > s.baseline_latency_ns * 5.0 && !s.timing_spoofed) {
      cpuid_lat = true;
    }
    if (s.timing_spoofed && s.personal_hv) {
      timing_and_hv = true;
    }
  }

  // Real temporal change only: consecutive samples with different payloads.
  int timeline_flips = 0;
  if (samples_.size() >= 2) {
    for (std::size_t i = 1; i < samples_.size(); ++i) {
      if (!same_trust_payload(samples_[i], samples_[i - 1])) {
        ++timeline_flips;
        r.reasons.push_back("timeline_flip@" +
                            std::to_string(samples_[i].tick));
      }
    }
  }

  auto add_reason = [&](bool on, const char* tag) {
    if (on) {
      r.reasons.push_back(tag);
    }
  };
  add_reason(vbs_off, "vbs_off");
  add_reason(hvci_off, "hvci_off");
  add_reason(secure_boot_off, "secure_boot_off");
  add_reason(attest_fail, "attest_fail");
  add_reason(smm, "smm_residual");
  add_reason(personal_hv, "personal_hv");
  add_reason(timing_spoof, "timing_spoof");
  add_reason(ept_hide, "ept_hide");
  add_reason(vtl1_miss, "vtl1_miss");
  add_reason(sk_dirty, "sk_dirty");
  add_reason(cpuid_lat, "cpuid_latency");
  add_reason(vbs_vs_hv, "vbs_vs_personal_hv");
  add_reason(timing_and_hv, "timing_and_personal_hv");

  r.policy_fails = (vbs_off ? 1 : 0) + (hvci_off ? 1 : 0) +
                   (secure_boot_off ? 1 : 0) + (attest_fail ? 1 : 0);
  r.spoof_signals = (personal_hv ? 1 : 0) + (timing_spoof ? 1 : 0) +
                    (ept_hide ? 1 : 0) + (vtl1_miss ? 1 : 0) +
                    (sk_dirty ? 1 : 0) + (smm ? 1 : 0) + (cpuid_lat ? 1 : 0);
  r.inconsistent_pairs =
      timeline_flips + (vbs_vs_hv ? 1 : 0) + (timing_and_hv ? 1 : 0);

  r.risk = r.policy_fails * 2.0 + r.spoof_signals * 1.5 +
           r.inconsistent_pairs * 2.5;

  if (r.risk >= 12.0 || (r.policy_fails >= 2 && r.spoof_signals >= 1)) {
    r.action = TrustAction::HardBlock;
    r.allow_ranked = false;
  } else if (r.risk >= 6.0 || r.policy_fails >= 1 || r.inconsistent_pairs >= 2) {
    r.action = TrustAction::BlockRanked;
    r.allow_ranked = false;
  } else if (r.risk >= 2.0 || r.spoof_signals >= 1) {
    r.action = TrustAction::SoftFlag;
    r.allow_ranked = true;  // flag but allow
  } else {
    r.action = TrustAction::Allow;
    r.allow_ranked = true;
  }

  std::ostringstream oss;
  oss << "samples=" << r.samples << " unique_union=1"
      << " policy_fails=" << r.policy_fails << " spoof=" << r.spoof_signals
      << " inconsistent=" << r.inconsistent_pairs << " flips=" << timeline_flips
      << " risk=" << r.risk << " action=" << static_cast<int>(r.action)
      << " ranked=" << (r.allow_ranked ? 1 : 0);
  r.detail = oss.str();
  return r;
}

// TrustAggregator::evaluate_world_timeline: Evaluate multi-sample trust timeline for ranked.
TrustAggregateResult TrustAggregator::evaluate_world_timeline(
    const sim::World& w, int ticks) {
  // Capture once per call when world is frozen: re-sampling identical state
  // does not inflate risk. Callers that want a real timeline must mutate
  // World between push() invocations.
  reset();
  if (ticks <= 0) {
    return evaluate();
  }
  // One snapshot of current state (tick 0). Extra tick count only notes intent
  // for multi-push callers; frozen re-sample is collapsed by union evaluate.
  push(capture(w, 0));
  // If ticks > 1, also push copies only when documenting sample budget — but
  // same payload → zero flips and union counts stay stable (same risk @ t=1 or t=3).
  for (int t = 1; t < ticks; ++t) {
    auto s = capture(w, t);
    // Skip pure duplicates of the last sample (no temporal information).
    if (!samples_.empty() && same_trust_payload(s, samples_.back())) {
      continue;
    }
    push(s);
  }
  // Ensure at least the first sample remains even if all identical.
  if (samples_.empty()) {
    push(capture(w, 0));
  }
  return evaluate();
}

// plant_hostile_trust_timeline: free function for this educational unit.
void plant_hostile_trust_timeline(sim::World& w) {
  // Multi-step red residual chain for aggregator demos:
  //  1) collapse platform trust (VBS/HVCI/secure boot off)
  //  2) start personal HV under collapsed policy
  //  3) spoof timing + EPT-hide AC pages
  //  4) break attestation / VTL1 / secure-kernel view
  //  5) inflate CPUID latency vs baseline (timing probe signal)
  w.trust.vbs = false;
  w.trust.hvci = false;
  w.trust.hvci_enabled = false;
  w.trust.secure_boot = false;
  w.try_start_personal_hv("lab-hv");
  w.trust.timing_spoofed = true;
  w.trust.ept_hide_ac_pages = true;
  w.trust.attestation_valid = false;
  w.trust.attestation_pcr_ok = false;
  w.vtl1_enclave_present = false;
  w.trust.secure_kernel_view_dirty = true;
  w.trust.baseline_latency_ns = 100.0;
  w.trust.cpuid_latency_ns = 900.0;
  w.smm_residual = false;  // pure HV path; SMM residual is separate family
  w.note("plant_hostile_trust_timeline");
}

}  // namespace depth
