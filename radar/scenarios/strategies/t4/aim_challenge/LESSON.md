# 60_aim_challenge — Aim challenge residual

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Multi-sample capture/serial aim fails server challenge

Blue: Multi-reason challenge+input+capture; fog

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::aim_challenge::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Aim challenge fail — Capture/serial residual aims well but fails multi-sample challenges.
2. Step 1: vision/HID path (capture card + serial MCU), no memory scar.
3. Step 2: multi-sample aim — looks accurate but fails server challenge.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.capture_card_present — HostTrust.capture_card_present = true
- push_input() — Push InputEvent (injected vs raw_hid provenance).
- aim_samples — Multi-sample AimSample list (camera vs server angles, challenge_passed).

Achieved when: `r.capture_or_serial && r.challenge_failed && r.sample_count >= 3 && r.failed_count >= 2`

## BLUE

Entry: `examples::aim_challenge::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Server aim challenge multi-sample — Flag multi-sample !challenge_passed with HID/capture correlate.
2. Narrator counter: Interest management residual — Fog still applies when challenge / input scar is present.
3. local `capture` init=w.trust.capture_card_present || w.desktop_duplication ||
                       w.external_display_clone
4. local `reasons` init=0
5. reads HostTrust platform fields
6. structural fog / stream surfaces
7. aim residual samples

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `challenge_fail`
- result field `bad_input`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- result field `failed_samples`
- local `capture` init=w.trust.capture_card_present || w.desktop_duplication ||
                       w.external_display_clone
- local `reasons` init=0
- reads HostTrust platform fields
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `(r.challenge_fail || r.bad_input || capture) && (r.multi_reason || r.challenge_fail)`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Feature cheats leave aim/input residuals. Require multi-reason (desync + challenge fail), never a single silent_aim bool echo.

## Run

```bash
./build/strategy_lab run 60_aim_challenge
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `aim_challenge/red_example.cpp` — full red multi-step
- `aim_challenge/blue_example.cpp` — full blue multi-reason
- `aim_challenge/pair.cpp` — StrategyEntry wiring + narrator
