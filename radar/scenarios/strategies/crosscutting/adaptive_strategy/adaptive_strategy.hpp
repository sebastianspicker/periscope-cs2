#pragma once

// Cross-cutting adaptive red team strategy layer.
// Probes blue sensors, selects optimal evasion techniques, varies patterns
// across ticks for multi-tick opsec, and manages C2 resilience.

#include "ac/types.hpp"
#include "sim/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ops_red {

// Which tier(s) the red team is currently operating at.
enum class ActiveTier : std::uint8_t {
  T0_Usermode = 0,
  T1_Syscall = 1,
  T2_Kernel = 2,
  T3_Hypervisor = 3,
  T4_DmaHardware = 4,
  Mixed = 5,  // multi-tier fallback chain active
};

// Profile of blue sensors detected through probing.
struct BlueSensorProfile {
  bool handle_graph_monitor = false;
  bool process_cooccurrence = false;
  bool driver_guard = false;
  bool callback_integrity = false;
  bool trust_policy = false;
  bool hv_probe = false;
  bool iommu_policy = false;
  bool fog_of_war = false;
  bool info_advantage_scorer = false;
  bool delayed_ban = false;
  int sensor_count = 0;
  std::string detail;
};

// Evasion probability assessment for a single technique.
struct TechniqueOdds {
  std::string technique_name;
  double survival_probability = 0.0;  // 0.0-1.0 against current sensor set
  std::vector<std::string> detected_by;  // which sensors detect this
};

// Assessment produced after selecting a delivery tier for the current sensors.
struct AdaptiveStrategyReport {
  ActiveTier chosen_tier = ActiveTier::T0_Usermode;
  bool adaptive_active = false;
  int sensors_profiled = 0;
  double evasion_probability = 0.0;
  bool multi_tick_opsec = false;
  int opsec_variant = 0;
  bool c2_fronting = false;
  bool c2_fallback = false;
  int c2_endpoints = 0;
  bool c2_oob = false;
  bool stealth_exfil = false;
  std::vector<TechniqueOdds> technique_odds;
  std::vector<std::string> recommended_techniques;
  std::vector<std::string> avoided_techniques;
  BlueSensorProfile sensor_profile;
  std::string detail;
};

// Multi-tier fallback decision.
struct TierFallbackDecision {
  ActiveTier primary = ActiveTier::T0_Usermode;
  ActiveTier fallback = ActiveTier::T0_Usermode;
  bool chain_ready = false;
  std::string reason;
};

// Adaptive red strategy orchestrator.
class AdaptiveStrategy {
 public:
  explicit AdaptiveStrategy(sim::World& world);

  /// Probe blue sensors by examining World state.
  BlueSensorProfile profile_blue_sensors();

  /// Assess evasion probability for each technique given current sensor profile.
  std::vector<TechniqueOdds> assess_technique_odds(
      const BlueSensorProfile& profile);

  /// Select optimal tier given sensor profile.
  ActiveTier select_optimal_tier(const BlueSensorProfile& profile);

  /// Make tier fallback decision.
  TierFallbackDecision decide_fallback(const BlueSensorProfile& profile,
                                       ActiveTier primary);

  /// Apply adaptive strategy: profile -> assess -> select -> avoid detected techs.
  AdaptiveStrategyReport apply();

  /// Enable multi-tick opsec (varying patterns per tick).
  void enable_multi_tick_opsec(int pattern_count = 4);

  /// Advance to next opsec pattern variant.
  void advance_opsec_variant();

  /// Enable C2 resilience chain.
  void enable_c2_resilience(bool fronting = true, bool fallback = true,
                            int fallback_count = 3, bool oob = false);

  /// Enable stealth exfiltration.
  void enable_stealth_exfiltration();

  /// Get current World state mutations.
  void apply_to_world();

 private:
  sim::World& world_;
  BlueSensorProfile cached_profile_;
  int current_variant_ = 0;
  int opsec_count_ = 4;
  AdaptiveStrategyReport last_report_{};
};

}  // namespace ops_red
