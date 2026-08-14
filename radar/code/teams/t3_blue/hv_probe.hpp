#pragma once

// Multi-invariant HV presence probes (vendor, latency, personal HV, timing spoof).

#include "ac/telemetry.hpp"
#include "sim/world.hpp"

#include <string>
#include <vector>

namespace t3_blue {

// Lab type `HvProbeSample` used by this educational unit.
struct HvProbeSample {
  bool cpuid_hypervisor_bit = false;
  char vendor[13] = {};
  double cpuid_latency_ns = 0;
  double baseline_latency_ns = 0;
};

// Aggregate outcome fields for `HvProbeResult` (lab narrative / tests).
struct HvProbeResult {
  bool anomaly = false;
  bool personal_hv = false;
  bool timing_spoof = false;
  bool latency_hit = false;
  bool vendor_hit = false;
  std::vector<std::string> reasons;
  double risk = 0;
  std::string detail;
};

// Lab type `HvProbe` used by this educational unit.
class HvProbe {
 public:
  explicit HvProbe(ac::ITelemetrySink& sink);
  void analyze(const HvProbeSample& sample);

  /// Capture sample from World trust model and analyze.
  HvProbeResult analyze_world(const sim::World& w);

  bool last_anomaly() const { return last_anomaly_; }

 private:
  ac::ITelemetrySink& sink_;
  bool last_anomaly_ = false;
};

}  // namespace t3_blue
