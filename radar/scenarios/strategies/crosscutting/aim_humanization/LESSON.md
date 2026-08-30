# 11_aim_humanization — Aim humanization

Family: Feature. Tiers: all. Area: xc/features. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Soft aim distributions look human

Blue: Don't rely on snap-only ML; add info-advantage

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::aim_humanization::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Aim humanization — Noise, Bezier, FOV limits, miss% — defeat naive snap detectors.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- add_net() — Plant NetFlow residual (radar SaaS / C2).
- push_input() — Push InputEvent (injected vs raw_hid provenance).
- inputs — InputEvent provenance (injected vs raw_hid).

Achieved when: `!r.soft_inhuman && r.soft_phase_samples >= 8`

## BLUE

Entry: `examples::aim_humanization::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Kinematics ML alone — Catches rage; struggles with soft aim + radar-only.
2. Narrator counter: Info-advantage features — Pair aim with server vision/sound truth + radar SaaS residual.
3. Multi-reason story: snap alone is weak on soft; residual strengthens.
4. local `huge` init=0
5. local `radar` init=net_looks_like_radar(w)
6. local `multibox` init=w.multibox_net_aim
7. local `aim_issues` init=aim_sample_issues(w)
8. local `reasons` init=0
9. aim residual samples

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `snap_flag`
- result field `residual_info`
- result field `residual_hit`
- result field `multi_reason`
- result field `detected`
- local `huge` init=0
- local `radar` init=net_looks_like_radar(w)
- local `multibox` init=w.multibox_net_aim
- local `aim_issues` init=aim_sample_issues(w)
- local `reasons` init=0

Win conditions for this pair:
- detected := `r.snap_flag`
- mitigated := `blue.residual_hit`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Feature cheats leave aim/input residuals. Require multi-reason (desync + challenge fail), never a single silent_aim bool echo.

## Run

```bash
./build/strategy_lab run 11_aim_humanization
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `aim_humanization/red_example.cpp` — full red multi-step
- `aim_humanization/blue_example.cpp` — full blue multi-reason
- `aim_humanization/pair.cpp` — StrategyEntry wiring + narrator
