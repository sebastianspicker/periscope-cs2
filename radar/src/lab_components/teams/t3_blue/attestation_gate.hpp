// attestation_gate.hpp — T3 blue ranked admission gate on AttestationQuote + TrustPolicy.
// Multi-reason: signature, PCR, host trust, personal HV, dual-view scars.

#pragma once

#include "sim/world.hpp"
#include "t3_blue/trust_policy.hpp"

#include <string>
#include <vector>

namespace t3_blue {

// AttestationQuote: lab type for this educational unit.
struct AttestationQuote {
  bool valid_signature = false;
  bool pcr_matches_known_good = false;
  HostTrustState host{};
  /// Optional lab residual flags (filled by check_world from World.trust).
  bool personal_hv = false;
  bool ept_hide = false;
  bool secure_kernel_dirty = false;
  bool timing_spoofed = false;
};

/// Multi-reason gate outcome (PolicyResult + expanded reasons).
struct AttestationGateReport {
  PolicyResult policy{};
  bool allow_ranked = false;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};

/// Optional hard gate for ranked entry when platform supports quotes.
class AttestationGate {
 public:
  explicit AttestationGate(TrustPolicy& policy);

  // Gate ranked play: require valid quote signature/PCR then TrustPolicy.
  // Multi-reason: accumulates signature/PCR/host/residual failures.
  PolicyResult admit_ranked(const AttestationQuote& quote);

  /// Full multi-reason admit with structured report (tests + PlatformAc).
  AttestationGateReport evaluate_quote(const AttestationQuote& quote);

  /// Convenience: build a quote from World trust and admit_ranked.
  PolicyResult check_world(const sim::World& w);

  /// Full multi-reason world check with residual scar correlation.
  AttestationGateReport evaluate_world(const sim::World& w);

  const AttestationGateReport& last_report() const { return last_; }

 private:
  TrustPolicy& policy_;
  mutable std::string reason_storage_;
  AttestationGateReport last_{};
};

}  // namespace t3_blue
