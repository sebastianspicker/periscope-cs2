#include "adaptive_strategy.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace ops_red {
namespace {

ActiveTier tier_for_technique(const std::string& technique_name) {
  if (technique_name == "handle proxy") {
    return ActiveTier::T0_Usermode;
  }
  if (technique_name == "syscall") {
    return ActiveTier::T1_Syscall;
  }
  if (technique_name == "kernel driver") {
    return ActiveTier::T2_Kernel;
  }
  if (technique_name == "HV") {
    return ActiveTier::T3_Hypervisor;
  }
  return ActiveTier::T4_DmaHardware;
}

void add_sensor(bool detected, const char* name, int& count,
                std::vector<std::string>& names) {
  if (detected) {
    ++count;
    names.emplace_back(name);
  }
}

}  // namespace

AdaptiveStrategy::AdaptiveStrategy(sim::World& world) : world_(world) {}

BlueSensorProfile AdaptiveStrategy::profile_blue_sensors() {
  BlueSensorProfile profile;
  std::vector<std::string> sensors;
  const bool ac_present = world_.ac_pid() != 0;
  const std::uint32_t game_pid = world_.game_pid();

  profile.handle_graph_monitor =
      game_pid != 0 && !world_.handles_to(game_pid).empty();
  profile.process_cooccurrence = ac_present;
  profile.driver_guard = !world_.driver_allowlist_sha.empty();
  profile.callback_integrity = world_.ac_callback_present;
  profile.trust_policy = world_.trust.vbs;
  profile.hv_probe = world_.trust.personal_hv_active;
  profile.iommu_policy = world_.trust.iommu_on;
  profile.fog_of_war = world_.client_entity_fidelity < 1.0f;
  profile.info_advantage_scorer = world_.lab_confidence > 0.0;
  profile.delayed_ban = world_.lab_delayed_ban_ready;

  add_sensor(profile.handle_graph_monitor, "handle graph", profile.sensor_count,
             sensors);
  add_sensor(profile.process_cooccurrence, "process cooccurrence",
             profile.sensor_count, sensors);
  add_sensor(profile.driver_guard, "driver guard", profile.sensor_count, sensors);
  add_sensor(profile.callback_integrity, "callback integrity", profile.sensor_count,
             sensors);
  add_sensor(profile.trust_policy, "trust policy", profile.sensor_count, sensors);
  add_sensor(profile.hv_probe, "HV probe", profile.sensor_count, sensors);
  add_sensor(profile.iommu_policy, "IOMMU policy", profile.sensor_count, sensors);
  add_sensor(profile.fog_of_war, "fog of war", profile.sensor_count, sensors);
  add_sensor(profile.info_advantage_scorer, "info advantage scorer",
             profile.sensor_count, sensors);
  add_sensor(profile.delayed_ban, "delayed ban", profile.sensor_count, sensors);

  profile.detail = ac_present ? "AC present; " : "AC absent; ";
  profile.detail += std::to_string(profile.sensor_count) + " sensors profiled";
  if (!sensors.empty()) {
    profile.detail += ": ";
    for (std::size_t index = 0; index < sensors.size(); ++index) {
      if (index != 0) {
        profile.detail += ", ";
      }
      profile.detail += sensors[index];
    }
  }

  cached_profile_ = profile;
  return profile;
}

std::vector<TechniqueOdds> AdaptiveStrategy::assess_technique_odds(
    const BlueSensorProfile& profile) {
  std::vector<TechniqueOdds> odds;

  TechniqueOdds handle_proxy{"handle proxy", 0.85, {}};
  if (profile.handle_graph_monitor) {
    handle_proxy.survival_probability = 0.6;
    handle_proxy.detected_by.emplace_back("handle graph");
  }
  odds.push_back(std::move(handle_proxy));

  TechniqueOdds syscall{"syscall", 0.7, {}};
  if (profile.callback_integrity) {
    syscall.survival_probability = 0.4;
    syscall.detected_by.emplace_back("callback integrity");
  }
  odds.push_back(std::move(syscall));

  TechniqueOdds kernel_driver{"kernel driver", 0.85, {}};
  if (profile.handle_graph_monitor) {
    kernel_driver.survival_probability = 0.9;
    kernel_driver.detected_by.emplace_back("handle graph");
  }
  if (profile.driver_guard) {
    kernel_driver.survival_probability = 0.3;
    kernel_driver.detected_by.emplace_back("driver guard");
  }
  odds.push_back(std::move(kernel_driver));

  TechniqueOdds hypervisor{"HV", 0.8, {}};
  if (profile.hv_probe) {
    hypervisor.survival_probability = 0.4;
    hypervisor.detected_by.emplace_back("HV probe");
  }
  odds.push_back(std::move(hypervisor));

  TechniqueOdds dma{"DMA", 0.7, {}};
  if (profile.iommu_policy) {
    dma.detected_by.emplace_back("IOMMU policy");
  }
  odds.push_back(std::move(dma));

  return odds;
}

ActiveTier AdaptiveStrategy::select_optimal_tier(
    const BlueSensorProfile& profile) {
  const auto odds = assess_technique_odds(profile);
  constexpr std::array<ActiveTier, 5> priority = {
      ActiveTier::T4_DmaHardware, ActiveTier::T3_Hypervisor,
      ActiveTier::T2_Kernel, ActiveTier::T1_Syscall, ActiveTier::T0_Usermode};

  for (const ActiveTier tier : priority) {
    const auto technique = std::find_if(
        odds.begin(), odds.end(), [tier](const TechniqueOdds& candidate) {
          return tier_for_technique(candidate.technique_name) == tier;
        });
    if (technique != odds.end() && technique->survival_probability > 0.5) {
      return tier;
    }
  }
  return ActiveTier::T0_Usermode;
}

TierFallbackDecision AdaptiveStrategy::decide_fallback(
    const BlueSensorProfile& profile, ActiveTier primary) {
  const auto odds = assess_technique_odds(profile);
  const auto fallback = std::max_element(
      odds.begin(), odds.end(), [primary](const TechniqueOdds& left,
                                          const TechniqueOdds& right) {
        const bool left_primary = tier_for_technique(left.technique_name) == primary;
        const bool right_primary = tier_for_technique(right.technique_name) == primary;
        if (left_primary != right_primary) {
          return left_primary;
        }
        return left.survival_probability < right.survival_probability;
      });

  TierFallbackDecision decision;
  decision.primary = primary;
  if (fallback != odds.end() &&
      tier_for_technique(fallback->technique_name) != primary) {
    decision.fallback = tier_for_technique(fallback->technique_name);
    decision.chain_ready = fallback->survival_probability > 0.5;
    decision.reason = "next-best technique is " + fallback->technique_name;
  } else {
    decision.reason = "no distinct fallback tier available";
  }
  return decision;
}

AdaptiveStrategyReport AdaptiveStrategy::apply() {
  const BlueSensorProfile profile = profile_blue_sensors();
  const std::vector<TechniqueOdds> odds = assess_technique_odds(profile);
  const ActiveTier chosen_tier = select_optimal_tier(profile);
  const TierFallbackDecision fallback = decide_fallback(profile, chosen_tier);

  AdaptiveStrategyReport report;
  report.chosen_tier = chosen_tier;
  report.adaptive_active = true;
  report.sensors_profiled = profile.sensor_count;
  report.technique_odds = odds;
  report.sensor_profile = profile;
  report.multi_tick_opsec = world_.multi_tick_opsec_active;
  report.opsec_variant = world_.opsec_pattern_variant;
  report.c2_fronting = world_.c2_domain_fronting_active;
  report.c2_fallback = world_.c2_fallback_chain_active;
  report.c2_endpoints = world_.c2_fallback_endpoint_count;
  report.c2_oob = world_.c2_out_of_band_active;
  report.stealth_exfil = world_.stealth_exfiltration_active;

  for (const TechniqueOdds& technique : odds) {
    if (technique.survival_probability > 0.5) {
      report.recommended_techniques.push_back(technique.technique_name);
    } else {
      report.avoided_techniques.push_back(technique.technique_name);
    }
    if (tier_for_technique(technique.technique_name) == chosen_tier) {
      report.evasion_probability = technique.survival_probability;
    }
  }
  report.detail = profile.detail + "; " + fallback.reason;

  last_report_ = report;
  apply_to_world();
  return last_report_;
}

void AdaptiveStrategy::enable_multi_tick_opsec(int pattern_count) {
  opsec_count_ = std::max(1, pattern_count);
  current_variant_ = 0;
  world_.multi_tick_opsec_active = true;
  world_.opsec_pattern_count = opsec_count_;
  world_.opsec_pattern_variant = current_variant_;
}

void AdaptiveStrategy::advance_opsec_variant() {
  current_variant_ = (current_variant_ + 1) % std::max(1, opsec_count_);
  world_.opsec_pattern_variant = current_variant_;
}

void AdaptiveStrategy::enable_c2_resilience(bool fronting, bool fallback,
                                             int fallback_count, bool oob) {
  world_.c2_domain_fronting_active = fronting;
  world_.c2_fronting_host = "cloudflare-cdn.lab";
  world_.c2_fallback_chain_active = fallback;
  world_.c2_fallback_endpoint_count = fallback_count;
  world_.c2_out_of_band_active = oob;
  world_.c2_oob_channel = oob ? "dns" : "";
}

void AdaptiveStrategy::enable_stealth_exfiltration() {
  world_.stealth_exfiltration_active = true;
  world_.stealth_exfiltrated_bytes = 0;
}

void AdaptiveStrategy::apply_to_world() {
  world_.adaptive_strategy_active = last_report_.adaptive_active;
  world_.blue_sensors_profiled = last_report_.sensors_profiled;
  world_.current_evasion_probability = last_report_.evasion_probability;
}

}  // namespace ops_red
