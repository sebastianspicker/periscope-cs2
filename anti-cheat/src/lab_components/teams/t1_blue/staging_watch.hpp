#pragma once

// Supportive T1 signal: short-lived stubs + private RX / manual map regions.

#include "ac/telemetry.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace t1_blue {

// Lab type `StagingObservation` used by this educational unit.
struct StagingObservation {
  std::uint32_t pid = 0;
  std::string image_name;
  bool has_private_rx = false;
  bool short_lived_stub = false;
};

// Lab type `StagingScanHit` used by this educational unit.
struct StagingScanHit {
  std::uint32_t pid = 0;
  std::string name;
  std::string reason;
  double risk = 1.5;
};

// Aggregate outcome fields for `StagingScanResult` (lab narrative / tests).
struct StagingScanResult {
  bool hit = false;
  std::vector<StagingScanHit> hits;
  std::string detail;
};

// Lab type `StagingWatch` used by this educational unit.
class StagingWatch {
 public:
  explicit StagingWatch(ac::ITelemetrySink& sink);
  void observe(const StagingObservation& obs);

  /// Scan World for staging scars (stub names, manual map, mapper_process).
  StagingScanResult scan_world(const sim::World& w);

  const StagingScanResult& last() const { return last_; }

 private:
  ac::ITelemetrySink& sink_;
  StagingScanResult last_{};
};

}  // namespace t1_blue
