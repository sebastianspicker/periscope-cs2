#pragma once

// T3 educational platform AC: ranked trust policy + HV probe + bridge scan.
// Simulated HostTrust multi-sensor.

#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "sim/world.hpp"

#include <string>
#include <vector>

namespace t3_blue {

// Multi-reason platform detection result used by demos and strategies.
struct PlatformDetection {
  bool ranked_allowed = true;
  bool hv_anomaly = false;
  bool bridge_hit = false;
  bool policy_fail = false;
  bool attest_fail = false;
  bool ept_dual_view = false;
  bool sk_dirty = false;
  bool trust_aggregate_block = false;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string detail;
};

class PlatformAc {
 public:
  explicit PlatformAc(sim::World& world, ac::ITelemetrySink& sink);

  void require_vbs_hvci(bool on = true);
  // Ranked admission probe: VBS/HVCI/secure boot + unexpected HV signals.
  PlatformDetection evaluate_ranked();
  PlatformDetection probe_hv();
  PlatformDetection scan_bridge();
  PlatformDetection full();

 private:
  sim::World& world_;
  ac::ITelemetrySink& sink_;
  ac::RiskAggregator risk_;
  bool require_trust_ = true;
};

}  // namespace t3_blue
