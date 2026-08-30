#include "blue_example.hpp"
#include <string>

namespace examples::adaptive_strategy {

BlueResult Blue::detect(const sim::World& w) noexcept {
  BlueResult result;

  if (!w.adaptive_strategy_active) {
    result.detail = "no adaptive strategy signature detected";
    return result;
  }

  if (w.multi_tick_opsec_active) {
    result.detected_opsec_pattern = true;
    ++result.detection_count;
  }

  if (w.opsec_pattern_count > 1) {
    result.detected_tier_hopping = true;
    ++result.detection_count;
  }

  if (w.c2_fallback_endpoint_count > 0 || w.c2_fallback_chain_active) {
    result.detected_c2_fallback = true;
    ++result.detection_count;
  }

  result.detail = "adaptive detection: tier_hop=" +
                  std::to_string(result.detected_tier_hopping) +
                  " opsec=" + std::to_string(result.detected_opsec_pattern) +
                  " c2_fallback=" + std::to_string(result.detected_c2_fallback) +
                  " count=" + std::to_string(result.detection_count);

  return result;
}

BlueResult Blue::mitigate(sim::World& w) noexcept {
  BlueResult result = detect(w);

  if (result.detection_count > 0) {
    w.adaptive_strategy_active = false;
    w.multi_tick_opsec_active = false;
    w.opsec_pattern_variant = 0;
    w.opsec_pattern_count = 0;
    w.c2_domain_fronting_active = false;
    w.c2_fallback_chain_active = false;
    w.c2_fallback_endpoint_count = 0;
    w.c2_out_of_band_active = false;
    w.stealth_exfiltration_active = false;
    w.current_evasion_probability = 0.0;
    w.blue_sensors_profiled = 0;
    result.detail += " [mitigated: reset to single tier, cleared adaptive scars]";
  }

  return result;
}

} // namespace
