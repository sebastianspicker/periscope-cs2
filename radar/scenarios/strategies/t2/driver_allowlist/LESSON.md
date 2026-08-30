# 33_driver_allowlist — Ranked driver allowlist

Family: Delivery. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Private memrw sha outside ranked allowlist

Blue: Strict allowlist: unknown driver hash is a scar

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::driver_allowlist::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Unknown memrw under ranked — Private driver hash not on allowlist; IOCTL R/W, no game handle.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- match_active — World.match_active = true
- ranked_strict_driver_allowlist — World.ranked_strict_driver_allowlist = true
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.provides_mem_rw — Driver.provides_mem_rw = true (mem R/W kernel path)
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- Device.mem_rw_ioctl — Device.mem_rw_ioctl = true (IOCTL mem path)
- device_ioctl_read() — IOCTL read via device — no game VM_READ handle required.
- spawn() — Spawn actor process on World process list.

Achieved when: `red.read_ok && !red.has_game_handle`

## BLUE

Entry: `examples::driver_allowlist::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Ranked driver allowlist — Strict mode: only known sha256 images may load during ranked.
2. Narrator counter: Policy idle — ranked_strict_driver_allowlist=false — allowlist not enforced.
3. local `allowed` init=std::find(w.driver_allowlist_sha.begin(), w.driver_allowlist_sha.end(),
                  d.sha256) != w.driver_allowlist_sha.end()
4. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `policy_active`
- result field `unknown_sha`
- local `allowed` init=std::find(w.driver_allowlist_sha.begin(), w.driver_allowlist_sha.end(),
                  d.sha256) != w.driver_allowlist_sha.end()
- inspects drivers/devices

Win conditions for this pair:
- detected := `policy_active && unknown_sha`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (match_active and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 33_driver_allowlist
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `driver_allowlist/red_example.cpp` — full red multi-step
- `driver_allowlist/blue_example.cpp` — full blue multi-reason
- `driver_allowlist/pair.cpp` — StrategyEntry wiring + narrator
