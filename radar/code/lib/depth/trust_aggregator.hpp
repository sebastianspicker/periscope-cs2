#pragma once

// Multi-invariant HV trust aggregation over time (ranked policy depth).
// Not a single World bool — aggregates inconsistent signals across samples.

#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace depth {

// Lab type `TrustSample` used by this educational unit.
struct TrustSample {
  int tick = 0;
  bool secure_boot = true;
  bool vbs = true;
  bool hvci = true;
  bool personal_hv = false;
  bool platform_hv = false;
  bool timing_spoofed = false;
  bool ept_hide_ac = false;
  bool attestation_valid = true;
  bool vtl1_enclave_present = true;
  bool secure_kernel_view_dirty = false;
  bool hvci_race = false;
  bool smm_residual = false;  // firmware-class residual (lab)
  double cpuid_latency_ns = 0;
  double baseline_latency_ns = 0;
  std::string hv_vendor;
};

enum class TrustAction : std::uint8_t {
  Allow = 0,
  SoftFlag,
  BlockRanked,
  HardBlock,
};

// Aggregate outcome fields for `TrustAggregateResult` (lab narrative / tests).
struct TrustAggregateResult {
  int samples = 0;
  int inconsistent_pairs = 0;  // e.g. claims VBS but personal HV / timing spoof
  int spoof_signals = 0;
  int policy_fails = 0;
  double risk = 0;
  TrustAction action = TrustAction::Allow;
  bool allow_ranked = true;
  std::vector<std::string> reasons;
  std::string detail;
};

// Lab type `TrustAggregator` used by this educational unit.
class TrustAggregator {
 public:
  void require_vbs(bool v) { require_vbs_ = v; }
  void require_hvci(bool v) { require_hvci_ = v; }
  void require_secure_boot(bool v) { require_secure_boot_ = v; }
  void require_no_smm_residual(bool v) { require_no_smm_ = v; }

  TrustSample capture(const sim::World& w, int tick) const;
  void push(const TrustSample& s);
  TrustAggregateResult evaluate() const;
  void reset() { samples_.clear(); }

  /// Snapshot current world. Identical frozen re-samples do **not** inflate
  /// risk (union scoring + skip duplicate payloads). For real timelines,
  /// mutate World and `push(capture(...))` between ticks, then `evaluate()`.
  TrustAggregateResult evaluate_world_timeline(const sim::World& w,
                                               int ticks = 3);

 private:
  std::vector<TrustSample> samples_;
  bool require_vbs_ = true;
  bool require_hvci_ = true;
  bool require_secure_boot_ = false;
  bool require_no_smm_ = true;
};

/// Plant multi-step red trust degradation for aggregator demos.
void plant_hostile_trust_timeline(sim::World& w);

}  // namespace depth
