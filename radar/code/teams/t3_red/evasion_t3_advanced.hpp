#pragma once

// Comprehensive T3 educational evasion orchestration. Sim::World only.

#include "t3_red/hv_radar.hpp"

#include <string>

namespace t3_red {

class EvasionT3Advanced {
 public:
  explicit EvasionT3Advanced(sim::World& world);

  /// Start the lab HV path and apply every advanced T3 residual.
  HvRadarReport max_hv_stealth(const std::string& vendor = "ACLABHV");

  /// Apply the complete deep T3 stealth chain, including Wave 11 residuals.
  HvRadarReport deep_hv_stealth(const std::string& vendor = "ACLABHV");

  const HvRadarReport& last_report() const { return radar_.last_report(); }
  HvRadar& radar() { return radar_; }

 private:
  HvRadar radar_;
};

}  // namespace t3_red
