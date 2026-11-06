#pragma once

// Educational risk aggregation over telemetry events (sim lab).
// Multi-signal bookkeeping: event_count, distinct_kinds, reasons.

#include "ac/telemetry.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace ac {

// Aggregated multi-signal risk for lab blue decisions.
struct RiskState {
  double score = 0.0;
  bool block_ranked = false;
  bool flag_overwatch = false;
  int event_count = 0;
  int distinct_kinds = 0;
  bool multi_signal = false;
  std::vector<std::string> reasons;
};

// Folds TelemetryEvent stream into RiskState (score, reasons, multi_signal).
class RiskAggregator {
 public:
  // Fold one event: bump score, track distinct kinds, append reasons.
  void ingest(const TelemetryEvent& ev);
  const RiskState& state() const { return state_; }
  // Clear score/reasons for a new scenario.
  void reset();

 private:
  RiskState state_{};
  std::unordered_set<int> kinds_seen_;
};

}  // namespace ac
