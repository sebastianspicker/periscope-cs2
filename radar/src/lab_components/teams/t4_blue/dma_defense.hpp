#pragma once

// T4 educational DMA residual defense — sim.
// Platform signal (weak) + structural fog (strong) + behavioral residual.

#include "sim/world.hpp"

#include <string>
#include <vector>

namespace t4_blue {

// Aggregate outcome fields for `BlueResult` (lab narrative / tests).
struct BlueResult {
  bool detected = false;   // DMA device + IOMMU off (client/platform signal)
  bool mitigated = false;  // structural fog / interest mgmt applied
  bool dma_device = false;
  bool iommu_off = false;
  std::string detail;
};

/// Multi-sensor T4 scan: platform + residual scars + structural + behavioral.
struct PlatformScan {
  bool ranked_allowed = true;
  bool dma_device = false;
  bool iommu_off = false;
  bool platform_signal = false;
  bool capture_card = false;
  bool desktop_dup = false;
  bool external_clone = false;
  bool lag_switch = false;
  bool dual_boot = false;
  bool aim_challenge_fail = false;
  bool multibox_aim = false;
  bool packet_loss = false;
  bool clipcursor = false;
  bool fog_applied = false;
  bool stream_encrypted = false;
  bool iommu_enforced = false;
  bool info_advantage_hit = false;
  bool structural_kill = false;
  double risk = 0;
  double ia_score = 0;
  int fog_replicated = 0;
  std::vector<std::string> reasons;
  std::string detail;
};

/// Platform signal (weak) + server residual (strong) against off-box DMA.
class DmaDefense {
 public:
  explicit DmaDefense(sim::World& world);

  /// Detect residual hardware scar on the host trust model.
  bool detect_platform_signal() const;

  /// Structural mitigation: LeakageScorer multi-step + IOMMU + ranked policy.
  void apply_interest_mgmt();

  /// Full blue step: detect + mitigate. Mutates world fog flag for demo.
  BlueResult detect();

  /// Platform-only: DMA device + IOMMU / ranked policy.
  PlatformScan scan_platform() const;

  /// Residual scars: capture card, desktop dup, lag switch, dual boot, etc.
  PlatformScan scan_residuals() const;

  /// Behavioral residual: feed InfoAdvantageScorer with synthetic radar frames.
  PlatformScan score_behavioral();

  /// Multi-sensor full scan + structural mitigate.
  /// Non-empty reasons and risk > 0 after a DMA scar.
  PlatformScan full();

 private:
  sim::World& world_;
};

/// Free-function form used by strategy wrappers.
BlueResult detect(sim::World& world);

}  // namespace t4_blue
