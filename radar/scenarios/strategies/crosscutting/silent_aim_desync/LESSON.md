# 62_silent_aim_desync — Silent aim desync

Family: Feature. Tiers: all. Area: xc/features. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: silent_aim_active with camera/server aim angle desync

Blue: Detect silent_aim flag or abs(camera-server) > threshold

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::silent_aim_desync::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Silent aim desync — Hitreg aim snaps server-side while camera stays off-target.
2. Step 1: silent aim active — camera stays "legit", hitreg snaps.
3. Step 2: multi-sample AimSample with camera ≠ server angles (≥4).
4. Step 3 (optional): small injected inputs that do NOT look like rage.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- silent_aim_active — World.silent_aim_active = true
- push_input() — Push InputEvent (injected vs raw_hid provenance).
- aim_samples — Multi-sample AimSample list (camera vs server angles, challenge_passed).

Achieved when: `r.silent_aim && r.desync && r.sample_count >= 2`

## BLUE

Entry: `examples::silent_aim_desync::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Camera vs server aim — Flag silent_aim_active or angle desync above threshold.
2. Narrator counter: Challenge multi-sample — Require multi-reason: desync count / challenge fails, not flag alone.
3. Detected: multi_reason OR (silent && angle) — not silent_flag alone.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `silent_flag`
- result field `angle_desync`
- result field `challenge_fail`
- result field `multi_reason`
- result field `detected`
- result field `desync_count`
- result field `failed_samples`
- local `kDesyncThreshold` init=0.25f
- local `primary` init=r.silent_flag || r.angle_desync
- local `support` init=r.challenge_fail || r.desync_count >= 2

Win conditions for this pair:
- detected := `r.multi_reason || (r.silent_flag && r.angle_desync)`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Feature cheats leave aim/input residuals. Require multi-reason (desync + challenge fail), never a single silent_aim bool echo.

## Run

```bash
./build/strategy_lab run 62_silent_aim_desync
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `silent_aim_desync/red_example.cpp` — full red multi-step
- `silent_aim_desync/blue_example.cpp` — full blue multi-reason
- `silent_aim_desync/pair.cpp` — StrategyEntry wiring + narrator
