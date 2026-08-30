# 10_web_phone_radar — Web/phone radar feed

Family: Feature. Tiers: T0-T2. Area: xc/features. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Stream entities to phone UI via PC reader + SaaS

Blue: Detect PC reader path ∧ SaaS (overlay clean narrative)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::web_phone_radar::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Web / phone radar multi-step — PC reader POSTs blips to radar SaaS; phone browser is UX only.
2. Step 1: reader process + VM_READ (PC still has the data path).
3. Step 2: net looks_like_radar_saas=true → radar-saas.example/ws
4. Step 3: optional second net POST blips.
5. Step 4: NO overlay (or empty overlays) — phone UX clean.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- spawn() — Spawn actor process on World process list.
- add_net() — Plant NetFlow residual (radar SaaS / C2).
- looks_like_radar_saas — looks_like_radar_saas=true scar

Achieved when: `r.handle_open && r.saas_net`

## BLUE

Entry: `examples::web_phone_radar::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Local reader still T0/T2 — Phone is UX; PC still has handle/driver.
2. Narrator counter: Handle ∧ SaaS correlation — Strong detect requires reader path + SaaS intel — not OR alone.
3. detected = handle_hit && saas_hit  // stronger than OR alone

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `handle_hit`
- result field `saas_hit`
- result field `overlay_clean`
- result field `multi_reason`
- result field `detected`
- reads handle graph via handles_to()
- inspects OverlayWindow list
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.handle_hit && r.saas_hit`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (open_process() and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 10_web_phone_radar
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0-T2
```

Open the pair sources beside this lesson:

- `web_phone_radar/red_example.cpp` — full red multi-step
- `web_phone_radar/blue_example.cpp` — full blue multi-reason
- `web_phone_radar/pair.cpp` — StrategyEntry wiring + narrator
