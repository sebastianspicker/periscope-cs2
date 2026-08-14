# 148_adaptive_strategy — Adaptive strategy

Family: Evasion. Tiers: crosscutting. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Adaptive multi-tier evasion — probes blue sensors, selects the optimal tier

Blue: Detect adaptive multi-tier evasion via tier-hopping, opsec, and C2 analysis

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::adaptive_strategy::Red::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Adaptive multi-tier evasion — profile blue sensors, score each tier, pick the best one, and layer in opsec and C2 resilience.
2. Uses `ops_red::AdaptiveStrategy` wrapper on the World.

Team / depth APIs used:
- `ops_red::AdaptiveStrategy`
- call `strategy.profile_blue_sensors()`
- call `strategy.assess_technique_odds()`
- call `strategy.select_optimal_tier()`
- call `strategy.decide_fallback()`
- call `strategy.enable_multi_tick_opsec(4)`
- call `strategy.enable_c2_resilience(...)`
- call `strategy.enable_stealth_exfiltration()`
- call `strategy.apply()`

Expanded team path (what the wrapper actually does on sim::World):
- Profiles blue sensor surface and scores each ActiveTier (T0_Usermode … T4_DmaHardware / Mixed).
- Picks the optimal tier and decides a fallback chain.
- Enables multi-tick opsec, C2 resilience with fallback endpoints, and stealth exfiltration.
- Sets World.adaptive_strategy_active = true and writes an AdaptiveStrategyReport residual.

World scars and lab surfaces (from shipped red code):
- adaptive_strategy_active — World.adaptive_strategy_active = true.
- multi_tick_opsec_active / opsec_pattern_count — opsec residuals.
- c2_fallback_endpoint_count / c2_fallback_chain_active — C2 residuals.
- w.note() — tier / evasion / profiled / opsec / c2 summary.

Achieved when: `w.adaptive_strategy_active` (pair marks red_achieved = true after apply).

## BLUE

Entry: `examples::adaptive_strategy::Blue::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Tier-hopping — switching active tier is itself a behavioral scar.
2. Narrator counter: Multi-tick opsec patterns and C2 fallback chains add corroborating reasons.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected_opsec_pattern`
- result field `detected_tier_hopping`
- result field `detected_c2_fallback`
- result field `detection_count`
- checks World.multi_tick_opsec_active / opsec_pattern_count
- checks World.c2_fallback_endpoint_count / c2_fallback_chain_active

Win conditions for this pair:
- detected := `blue.detection_count > 0`
- mitigated := `mitigation.detection_count > 0` (Blue::mitigate clears the adaptive scars and resets to a single tier)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

A cheat that adapts to its detector is detectable precisely because adaptation is noisy: tier hops, repeated opsec ticks, and C2 fallback endpoints are themselves residuals. Blue's response is to force the red actor back to a single tier and clear the adaptive state — resetting posture instead of chasing one technique.

## Run

```bash
./build/strategy_lab run 148_adaptive_strategy
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `adaptive_strategy/red_example.cpp` — full red multi-step
- `adaptive_strategy/blue_example.cpp` — full blue multi-reason
- `adaptive_strategy/pair.cpp` — StrategyEntry wiring + narrator
