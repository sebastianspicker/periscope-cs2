# 74_overwatch_queue — Overwatch delayed queue

Family: Detection. Tiers: all. Area: xc/ops. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Mild handle + no_rage + second weak (vpn/report)

Blue: weak_handle + multi-invariant; multi_reason handle&&overwatch

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::overwatch_queue::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Legit-looking weak multi-scar — Mild handle + no rage + optional vpn/report second signal.
2. Step 1: legit-looking play — no silent aim, no rage input storm.
3. Step 2: mild scar only — short-lived external handle (weak signal).
4. Step 3: optional second weak signal — low VPN residual (not full farm).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- silent_aim_active — World.silent_aim_active = false
- vpn_proxy_active — World.vpn_proxy_active = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.
- push_input() — Push InputEvent (injected vs raw_hid provenance).
- add_account() — Plant Account row (hwid/payment/ip/report_count graph).

Achieved when: `r.mild_handle && r.no_rage`

## BLUE

Entry: `examples::overwatch_queue::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Overwatch / delayed review queue — weak_handle + multi-invariant; multi_reason (not queue alone).
2. Step 1: weak handle signal on game process.
3. Step 2: delayed multi-invariant path with FP budget — not instant ban.
4. Step 3: only queue when a weak signal is present — not queue alone.

Team / depth sensors:
- `depth::MultiInvariantScorer`
  - RichDemoFrame path: vision_hits, latency, peer reports, delayed action / overwatch threshold.
- `depth::RichDemoFrame`
- `depth::apply_overwatch_to_world`
- `depth::DelayedAction`

Multi-reason / result fields and sensors:
- result field `weak_handle`
- result field `overwatch`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.multi_reason`
- mitigated := `r.multi_reason`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (silent_aim_active and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 74_overwatch_queue
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `overwatch_queue/red_example.cpp` — full red multi-step
- `overwatch_queue/blue_example.cpp` — full blue multi-reason
- `overwatch_queue/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/multi_invariant_scorer.hpp`
