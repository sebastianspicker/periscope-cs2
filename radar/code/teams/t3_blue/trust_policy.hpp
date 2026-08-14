#pragma once

// Primary T3 counter: competitive play requires known-good trust state.

#include "ac/telemetry.hpp"
#include "sim/world.hpp"

#include <string>

namespace t3_blue {

// Lab type `HostTrustState` used by this educational unit.
struct HostTrustState {
  bool secure_boot = false;
  bool vbs = false;
  bool hvci = false;
  bool unexpected_hypervisor = false;
};

// Aggregate outcome fields for `PolicyResult` (lab narrative / tests).
struct PolicyResult {
  bool allow_ranked = true;
  const char* reason = "ok";
  double risk = 0;
};

// Lab type `TrustPolicy` used by this educational unit.
class TrustPolicy {
 public:
  explicit TrustPolicy(ac::ITelemetrySink& sink);

  void require_vbs(bool v) { require_vbs_ = v; }
  void require_hvci(bool v) { require_hvci_ = v; }
  void require_secure_boot(bool v) { require_secure_boot_ = v; }

  // Admit/deny ranked from HostTrustState posture flags.
  PolicyResult evaluate_ranked(const HostTrustState& host);

  /// Capture HostTrustState from World.
  HostTrustState from_world(const sim::World& w) const;

  /// Evaluate ranked policy directly from World trust fields.
  PolicyResult evaluate_world(const sim::World& w);

 private:
  ac::ITelemetrySink& sink_;
  bool require_vbs_ = true;
  bool require_hvci_ = true;
  bool require_secure_boot_ = false;
};

}  // namespace t3_blue
