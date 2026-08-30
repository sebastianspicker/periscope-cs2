#include "red_example.hpp"
#include "adaptive_strategy.hpp"
#include <string>

namespace examples::adaptive_strategy {

void Red::apply(sim::World& w) noexcept {
  ops_red::AdaptiveStrategy strategy(w);

  ops_red::BlueSensorProfile profile = strategy.profile_blue_sensors();
  strategy.assess_technique_odds(profile);
  ops_red::ActiveTier chosen = strategy.select_optimal_tier(profile);
  strategy.decide_fallback(profile, chosen);
  strategy.enable_multi_tick_opsec(4);
  strategy.enable_c2_resilience(true, true, 3, false);
  strategy.enable_stealth_exfiltration();
  ops_red::AdaptiveStrategyReport report = strategy.apply();

  w.adaptive_strategy_active = true;

  std::string tier_str;
  switch (report.chosen_tier) {
    case ops_red::ActiveTier::T0_Usermode: tier_str = "T0_Usermode"; break;
    case ops_red::ActiveTier::T1_Syscall: tier_str = "T1_Syscall"; break;
    case ops_red::ActiveTier::T2_Kernel: tier_str = "T2_Kernel"; break;
    case ops_red::ActiveTier::T3_Hypervisor: tier_str = "T3_Hypervisor"; break;
    case ops_red::ActiveTier::T4_DmaHardware: tier_str = "T4_DmaHardware"; break;
    case ops_red::ActiveTier::Mixed: tier_str = "Mixed"; break;
  }

  w.note("adaptive_strategy: tier=" + tier_str +
         " evasion=" + std::to_string(report.evasion_probability) +
         " profiled=" + std::to_string(report.sensors_profiled) +
         " multi_tick_opsec=" + std::to_string(report.multi_tick_opsec) +
         " c2_resilient=" + std::to_string(report.c2_fallback));
}

} // namespace
