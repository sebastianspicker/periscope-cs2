#pragma once

// Multi-reason blue detector builder.
// Pairs accumulate weighted SignalHits; finish() produces a BlueOutcome
// with detected/mitigated thresholds. Fully deterministic — no stubs.

#include "sim/world.hpp"
#include "strategies/strategy_types.hpp"

#include <string>
#include <utility>
#include <vector>

namespace strategies {

/// Builder for multi-reason blue detection used by strategy pairs.
class MultiReasonDetector {
 public:
  explicit MultiReasonDetector(const char* strategy_id = "pair");

  MultiReasonDetector& set_detect_min_signals(int n);
  MultiReasonDetector& set_mitigate_min_signals(int n);
  MultiReasonDetector& set_detect_min_risk(double r);
  MultiReasonDetector& set_mitigate_min_risk(double r);
  MultiReasonDetector& set_require_specific_scar(bool v);

  /// Add a conditional signal; only recorded when `present` is true.
  MultiReasonDetector& signal(bool present, std::string reason, double weight = 0.24);

  /// Always-recorded note (still counts as a signal if weight > 0).
  MultiReasonDetector& note(std::string reason, double weight = 0.0);

  /// Mark that a strategy-specific scar was observed (gates detection).
  MultiReasonDetector& specific_scar(bool present, std::string reason = {},
                                     double weight = 0.30);

  /// Apply ranked deny when mitigate thresholds will fire.
  MultiReasonDetector& deny_ranked_on_mitigate(bool v);

  /// Finish scoring and optionally mutate World (ranked deny).
  BlueOutcome finish(sim::World* w = nullptr) const;

  /// Access accumulated hits (for tests / diagnostics).
  const std::vector<SignalHit>& hits() const { return hits_; }
  int signal_count() const;
  double risk() const;
  bool has_specific_scar() const { return specific_scar_; }

 private:
  const char* id_ = "pair";
  int detect_min_signals_ = 1;
  int mitigate_min_signals_ = 3;
  double detect_min_risk_ = 0.0;
  double mitigate_min_risk_ = 0.55;
  bool require_specific_ = false;
  bool specific_scar_ = false;
  bool deny_ranked_ = true;
  std::vector<SignalHit> hits_;
};

/// Free-function risk curve matching legacy examples::support::risk_for.
double risk_for_signals(int signals, double base = 0.18, double step = 0.24);

/// Append a signal when present (legacy-compatible helper).
void add_signal(int& signals, std::vector<std::string>& reasons, bool present,
                const char* reason);

/// Weighted variant that also accumulates risk.
void add_weighted_signal(BlueOutcome& out, bool present, std::string reason,
                         double weight);

}  // namespace strategies
