// staging_detector.hpp — T1 blue staging sensor: private RX / short-lived stub on processes.
// inspect(pid) for one actor; scan_world() walks StagingWatch hits.

#pragma once

#include "ac/telemetry.hpp"
#include "sim/world.hpp"
#include "t1_blue/staging_watch.hpp"

#include <string>

namespace t1_blue {

// StagingFinding: lab type for this educational unit.
struct StagingFinding {
  bool hit = false;
  std::string detail;
  double risk = 0;
};

// StagingDetector: lab type for this educational unit.
class StagingDetector {
 public:
  explicit StagingDetector(ac::ITelemetrySink& sink);

  /// Inspect a single pid with explicit flags (unit/proto path).
  StagingFinding inspect(const sim::World& w, std::uint32_t pid, bool private_rx,
                         bool short_lived_stub);

  /// Full World scan via StagingWatch.
  StagingFinding scan_world(const sim::World& w);

 private:
  ac::ITelemetrySink& sink_;
  StagingWatch watch_;
};

}  // namespace t1_blue
