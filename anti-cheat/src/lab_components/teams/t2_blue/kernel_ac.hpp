#pragma once

// Full T2 AC agent: driver/BYOVD/device/callback suite on sim::World.
// Handle graph empty is expected for pure T2.

#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "sim/world.hpp"
#include "t2_blue/byovd_blocklist.hpp"
#include "t2_blue/callback_integrity.hpp"
#include "t2_blue/device_watch.hpp"
#include "t2_blue/driver_guard.hpp"
#include "t2_red/callback_strip_sim.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace t2_blue {

// Aggregate outcome fields for `KernelDetection` (lab narrative / tests).
struct KernelDetection {
  bool byovd = false;
  bool unknown_memrw_driver = false;
  bool suspicious_device = false;
  bool callback_tamper = false;
  bool any_handle = false;  // unexpected pure-T2 contamination
  bool pool_tag = false;
  bool etw_ti = false;
  bool mitigated = false;
  double risk = 0;
  std::vector<std::string> reasons;
  std::string summary;
};

// Multi-sensor educational blue agent `KernelAc` — reasons/risk on World scars.
class KernelAc {
 public:
  explicit KernelAc(sim::World& world, ac::ITelemetrySink& sink);

  void blocklist_add(std::string sha);

  /// Legacy API used by duel: pass callback states explicitly.
  KernelDetection scan(const t2_red::CallbackState& cbs,
                       const t2_red::CallbackState& baseline);

  /// Primary: full multi-sensor scan from World only.
  KernelDetection full_scan();

  /// Apply BYOVD mitigate (block IOCTL + ranked deny).
  bool mitigate();

  const ac::RiskState& risk() const { return risk_.state(); }

 private:
  sim::World& world_;
  ac::ITelemetrySink& sink_;
  ac::RiskAggregator risk_;
  ByovdBlocklist blocklist_;
  DriverGuard drivers_;
  DeviceWatch devices_;
  CallbackIntegrity callbacks_;
  std::uint32_t game_pid_ = 0;
};

}  // namespace t2_blue
