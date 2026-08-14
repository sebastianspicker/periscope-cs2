#pragma once

// Server ban correlation for the educational lab: combine client risk with
// residual server scores (info-advantage) into a multi-signal BanDecision.
// Simulated policy glue — educational delayed-ban ladder only.

#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace server {

// Action ladder for correlated residual enforcement (lab policy).
enum class BanAction : std::uint8_t {
  None,
  FlagOverwatch,
  SoftRestrict,
  DelayedBanCandidate,
};

// Aggregate ban outcome: action thresholds plus multi-signal bookkeeping.
struct BanDecision {
  BanAction action = BanAction::None;
  double score = 0;
  const char* reason = "";
  int signal_count = 0;              // independent contributing families
  bool multi_signal = false;         // signal_count >= 2
  std::vector<std::string> reasons;  // human-readable family tags
};

/// Combines client telemetry risk with optional server scores.
///
/// Independent signal families (each counted at most once):
/// - client_risk: RiskState.score > 0
/// - block_ranked: RiskState.block_ranked
/// - flag_overwatch: RiskState.flag_overwatch
/// - info_advantage: info_advantage_score > 0 (one family; high vs low is a tag only)
///
/// Action ladder (first match wins):
/// 1. block_ranked → SoftRestrict
/// 2. combined score (client + ia) >= 10 → DelayedBanCandidate
/// 3. flag_overwatch OR ia >= 3 → FlagOverwatch
/// 4. else → None
class BanCorrelator {
 public:
  BanDecision evaluate(const ac::RiskState& client_risk,
                       double info_advantage_score) const;
};

}  // namespace server
