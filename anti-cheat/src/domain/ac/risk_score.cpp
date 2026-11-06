// risk_score.cpp — multi-signal RiskAggregator / risk scoring for blue events.
// ingest() folds EventKind deltas; state() surfaces lab risk for ban correlators.

#include "ac/risk_score.hpp"

namespace ac {

namespace {

// Policy kinds that force a ranked-match block in lab blue scoring.
bool forces_block_ranked(EventKind kind) {
  return kind == EventKind::ByovdBlocked || kind == EventKind::TrustPolicyFail;
}

// Kinds that alone are enough to raise the overwatch flag (info-advantage path).
bool forces_overwatch(EventKind kind) {
  return kind == EventKind::InfoAdvantageHit ||
         kind == EventKind::BridgeSuspected ||
         kind == EventKind::HvProbeAnomaly;
}

// Default reason label when the event carries no detail string.
const char* default_reason_for(EventKind kind) {
  switch (kind) {
    case EventKind::HandleToGame:
      return "handle_to_game";
    case EventKind::ProcessCoRun:
      return "process_co_run";
    case EventKind::DriverLoad:
      return "driver_load";
    case EventKind::ByovdBlocked:
      return "byovd_blocked";
    case EventKind::DeviceOpen:
      return "device_open";
    case EventKind::TrustPolicyFail:
      return "trust_policy_fail";
    case EventKind::HvProbeAnomaly:
      return "hv_probe_anomaly";
    case EventKind::BridgeSuspected:
      return "bridge_suspected";
    case EventKind::InfoAdvantageHit:
      return "info_advantage_hit";
    case EventKind::Generic:
      return "generic";
  }
  return "event";
}

}  // namespace

// RiskAggregator::ingest: Fold a telemetry event into aggregate risk score.
// Accumulates risk_delta, tracks distinct EventKinds, retains reasons, and
// flips block_ranked / flag_overwatch for the policy kinds downstream consumers
// (BanCorrelator, shared tests) already depend on.
void RiskAggregator::ingest(const TelemetryEvent& ev) {
  state_.score += ev.risk_delta;
  ++state_.event_count;
  kinds_seen_.insert(static_cast<int>(ev.kind));
  state_.distinct_kinds = static_cast<int>(kinds_seen_.size());

  if (!ev.detail.empty()) {
    state_.reasons.push_back(ev.detail);
  } else {
    state_.reasons.emplace_back(default_reason_for(ev.kind));
  }

  if (forces_block_ranked(ev.kind)) {
    state_.block_ranked = true;
  }
  // Overwatch: explicit info-advantage / HV / bridge signals, or cumulative score.
  if (forces_overwatch(ev.kind) || state_.score >= 5.0) {
    state_.flag_overwatch = true;
  }
  // Multi-signal once we have ≥2 events or ≥2 distinct kinds (same rule both ways).
  state_.multi_signal = state_.distinct_kinds >= 2 || state_.event_count >= 2;
}

// RiskAggregator::reset: Clear aggregate score/state for a new lab scenario.
void RiskAggregator::reset() {
  state_ = {};
  kinds_seen_.clear();
}

}  // namespace ac
