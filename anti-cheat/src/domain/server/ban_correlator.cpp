// ban_correlator.cpp — multi-signal BanCorrelator (delayed ban / confidence fusion).
// Structural blue: correlates weak signals over time, not instant rageban.

#include "server/ban_correlator.hpp"

namespace server {
namespace {

// Treat non-finite or negative residual scores as zero contribution.
double sanitize_ia(double info_advantage_score) {
  if (!(info_advantage_score > 0.0) || info_advantage_score != info_advantage_score) {
    return 0.0;
  }
  if (info_advantage_score > 1.0e9) {
    return 1.0e9;
  }
  return info_advantage_score;
}

double sanitize_client_score(double score) {
  if (!(score > 0.0) || score != score) {
    return 0.0;
  }
  if (score > 1.0e9) {
    return 1.0e9;
  }
  return score;
}

}  // namespace

// BanCorrelator::evaluate: fuse client RiskState with info-advantage residual.
BanDecision BanCorrelator::evaluate(const ac::RiskState& client_risk,
                                    double info_advantage_score) const {
  BanDecision d;

  const double client_score = sanitize_client_score(client_risk.score);
  const double ia = sanitize_ia(info_advantage_score);
  d.score = client_score + ia;

  // Count independent contributing signal families (always compute, no double-count).
  if (client_score > 0.0) {
    d.signal_count += 1;
    d.reasons.push_back("client_risk");
  }
  if (client_risk.block_ranked) {
    d.signal_count += 1;
    d.reasons.push_back("block_ranked");
  }
  if (client_risk.flag_overwatch) {
    d.signal_count += 1;
    d.reasons.push_back("flag_overwatch");
  }
  // Info-advantage is ONE residual family (not split by high/low thresholds).
  if (ia > 0.0) {
    d.signal_count += 1;
    if (ia >= 3.0) {
      d.reasons.push_back("info_advantage_high");
    } else {
      d.reasons.push_back("info_advantage");
    }
  }

  // Action ladder: trust/byovd soft block → delayed ban → overwatch → clean.
  if (client_risk.block_ranked) {
    d.action = BanAction::SoftRestrict;
    d.reason = "trust_or_byovd_policy";
  } else if (d.score >= 10.0) {
    d.action = BanAction::DelayedBanCandidate;
    d.reason = "correlated_high_risk";
  } else if (client_risk.flag_overwatch || ia >= 3.0) {
    d.action = BanAction::FlagOverwatch;
    d.reason = "info_advantage_or_risk_flag";
  } else {
    d.action = BanAction::None;
    d.reason = "clean";
  }

  d.multi_signal = (d.signal_count >= 2);
  return d;
}

}  // namespace server
