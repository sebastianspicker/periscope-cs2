# 79_speedhack_timescale — Speedhack time scale

Family: Feature. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: time_scale = 1.5 or 2.0 (QPC speedhack)

Blue: Detect time_scale != 1.0 (small epsilon)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::speedhack_timescale::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Speedhack time scale — Set time_scale to 1.5 or 2.0 (QPC / tick speedhack).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- time_scale — World.time_scale = kSpeedhackScale

Achieved when: `std::abs(w.time_scale - 1.0) > kEpsilon`

## BLUE

Entry: `examples::speedhack_timescale::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Time scale integrity — Detect |time_scale - 1.0| > epsilon.
2. Sensor: multi-clock consistency (QPC vs wall / tick rate). Sim models the
3. local `kEpsilon` init=1e-3

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `time_scale`
- result field `delta`
- result field `detected`
- result field `mitigated`
- local `kEpsilon` init=1e-3

Win conditions for this pair:
- detected := `out.delta > kEpsilon`
- mitigated := `out.detected`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (time_scale and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 79_speedhack_timescale
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `speedhack_timescale/red_example.cpp` — full red multi-step
- `speedhack_timescale/blue_example.cpp` — full blue multi-reason
- `speedhack_timescale/pair.cpp` — StrategyEntry wiring + narrator
