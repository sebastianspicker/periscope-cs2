// attestation_gate.cpp — T3 blue multi-reason ranked admission gate.
// check_world maps World.trust into quote fields then evaluate_quote.

#include "t3_blue/attestation_gate.hpp"

#include <sstream>

namespace t3_blue {

AttestationGate::AttestationGate(TrustPolicy& policy) : policy_(policy) {}

// AttestationGate::evaluate_quote: Multi-reason gate on signature/PCR/host/residuals.
AttestationGateReport AttestationGate::evaluate_quote(const AttestationQuote& quote) {
  last_ = {};
  last_.allow_ranked = true;

  if (!quote.valid_signature) {
    last_.allow_ranked = false;
    last_.risk += 5.0;
    last_.reasons.emplace_back("attestation_signature_invalid");
  }
  if (!quote.pcr_matches_known_good) {
    last_.allow_ranked = false;
    last_.risk += 4.0;
    last_.reasons.emplace_back("attestation_pcr_mismatch");
  }
  if (quote.personal_hv) {
    last_.allow_ranked = false;
    last_.risk += 4.0;
    last_.reasons.emplace_back("personal_hv_active");
  }
  if (quote.ept_hide) {
    last_.allow_ranked = false;
    last_.risk += 2.5;
    last_.reasons.emplace_back("ept_hide_ac_pages");
  }
  if (quote.secure_kernel_dirty) {
    last_.allow_ranked = false;
    last_.risk += 2.0;
    last_.reasons.emplace_back("secure_kernel_view_dirty");
  }
  if (quote.timing_spoofed) {
    last_.allow_ranked = false;
    last_.risk += 1.5;
    last_.reasons.emplace_back("timing_spoofed");
  }

  // Host trust policy (VBS/HVCI/secure boot/unexpected HV).
  auto host_pol = policy_.evaluate_ranked(quote.host);
  if (!host_pol.allow_ranked) {
    last_.allow_ranked = false;
    last_.risk += host_pol.risk > 0 ? host_pol.risk : 3.0;
    last_.reasons.emplace_back(std::string("host_policy:") + host_pol.reason);
  }

  if (last_.reasons.empty()) {
    last_.reasons.emplace_back("attestation_ok");
    last_.risk = 0;
    last_.allow_ranked = true;
  }

  std::ostringstream oss;
  oss << "attest allow=" << (last_.allow_ranked ? 1 : 0)
      << " risk=" << last_.risk << " reasons=" << last_.reasons.size();
  for (const auto& r : last_.reasons) {
    oss << " |" << r;
  }
  last_.detail = oss.str();

  // PolicyResult::reason is const char* — keep a durable member copy and also
  // prefer long-lived literals for the common deny paths.
  if (last_.allow_ranked) {
    reason_storage_ = "ok";
  } else if (!quote.valid_signature || !quote.pcr_matches_known_good) {
    reason_storage_ = "attestation_invalid";
  } else if (!last_.reasons.empty()) {
    reason_storage_.clear();
    for (std::size_t i = 0; i < last_.reasons.size(); ++i) {
      if (i) reason_storage_ += ";";
      reason_storage_ += last_.reasons[i];
    }
  } else {
    reason_storage_ = "attestation_invalid";
  }

  last_.policy =
      PolicyResult{last_.allow_ranked, reason_storage_.c_str(), last_.risk};
  return last_;
}

// AttestationGate::admit_ranked: Gate ranked play on AttestationQuote + TrustPolicy.
PolicyResult AttestationGate::admit_ranked(const AttestationQuote& quote) {
  return evaluate_quote(quote).policy;
}

// AttestationGate::evaluate_world: Map World.trust scars into multi-reason gate.
AttestationGateReport AttestationGate::evaluate_world(const sim::World& w) {
  AttestationQuote q;
  q.valid_signature = w.trust.attestation_valid;
  q.pcr_matches_known_good = w.trust.attestation_pcr_ok;
  q.host = policy_.from_world(w);
  q.personal_hv = w.trust.personal_hv_active;
  q.ept_hide = w.trust.ept_hide_ac_pages;
  q.secure_kernel_dirty = w.trust.secure_kernel_view_dirty;
  q.timing_spoofed = w.trust.timing_spoofed;
  return evaluate_quote(q);
}

// AttestationGate::check_world: Build AttestationQuote from World.trust and admit.
PolicyResult AttestationGate::check_world(const sim::World& w) {
  return evaluate_world(w).policy;
}

}  // namespace t3_blue
