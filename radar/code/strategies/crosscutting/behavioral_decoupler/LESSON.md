# 149_behavioral_decoupler — Behavioral decoupler

Family: Evasion. Tiers: crosscutting. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Behavioral decoupling — frame skip, entity omission, blind spots, position fuzzing

Blue: Detect behavioral decoupling via skip patterns, latency consistency, fuzz detection, and VACnet risk

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::behavioral_decoupler::Red::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Behavioral decoupling — configure skip/omit/fuzz rates and blind spots, then process a batch so telemetry no longer correlates with the aim.
2. Uses `features::BehavioralDecoupler` wrapper on the World.

Team / depth APIs used:
- `features::BehavioralDecoupler`
- call `decoupler.set_config()`
- call `decoupler.process_batch()`
- call `decoupler.humanize_counter_strafe()`
- call `decoupler.report()`

Expanded team path (what the wrapper actually does on sim::World):
- Set frame_skip_rate / entity_skip_rate / position_noise_px / latency_ticks range / counter_strafe_jitter / blind-spot ticks.
- Process a 100-entity batch and humanize counter-strafe samples.
- Publish a report (frames_skipped / entities_omitted / entities_fuzzed / avg_latency_ticks / vacnet_risk_score) into the World log.

World scars and lab surfaces (from shipped red code):
- w.log — "behavioral_decoupler:" residual entries with frames_skipped, omitted, fuzzed, latency, risk.

Achieved when: `red.apply()` completes and logs the report (pair marks red_achieved = true).

## BLUE

Entry: `examples::behavioral_decoupler::Blue::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Skip patterns — frame/entity omission leaves a statistical signature.
2. Narrator counter: Latency consistency and fuzz patterns corroborate.
3. Narrator counter: VACnet risk — a learned behavioral risk score is the strongest reason.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected_skip_patterns`
- result field `detected_latency_consistency`
- result field `detected_fuzz_pattern`
- result field `detected_vacnet_risk`
- result field `detection_count`
- scans World.log for the behavioral_decoupler residual

Win conditions for this pair:
- detected := `blue.detection_count > 0`
- mitigated := `mitigation.detection_count > 0` (Blue::mitigate clears the behavioral scars from the World log)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Behavioral decoupling trades mechanical detection for statistical visibility: once you add skip rates, latency jitter, and fuzzing, the telemetry stream itself becomes anomalous. Real-world behavioral detection systems (VACnet-style ML) are exactly what this pair models, and the blue mitigation is a memory-cleanup pass over the residual log.

## Run

```bash
./build/strategy_lab run 149_behavioral_decoupler
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `behavioral_decoupler/red_example.cpp` — full red multi-step
- `behavioral_decoupler/blue_example.cpp` — full blue multi-reason
- `behavioral_decoupler/pair.cpp` — StrategyEntry wiring + narrator
