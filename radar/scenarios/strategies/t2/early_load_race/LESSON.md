# 46_early_load_race — Early driver load race

Family: Delivery. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Memrw loads before AC (load_order < ac_driver_load_order)

Blue: Audit non-AC memrw with load_order before AC

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::early_load_race::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Early load race — Boot-order memrw before AC: load_order=10 vs ac_driver_load_order.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- Driver.signer — Driver.signer = "unknown"
- Driver.boot_start — Driver.boot_start = true
- Driver.byovd_known_bad — Driver.byovd_known_bad = false
- Driver.is_ac — Driver.is_ac = false
- Driver.is_bridge — Driver.is_bridge = false
- Driver.provides_mem_rw — Driver.provides_mem_rw = true
- Driver.load_order — Driver.load_order = r.load_order
- load_driver() — Load Driver into World.drivers (kernel image scar).
- ac_driver_load_order — Compare driver load_order vs World.ac_driver_load_order (early-load race)
- create_device() — Create Device node linked to driver.
- Device.mem_rw_ioctl — Device.mem_rw_ioctl = true (IOCTL mem path)

Achieved when: `loaded := true` (pair maps red_achieved := `red.loaded && red.device_created &&
                   red.load_order < red.ac_load_order`)

## BLUE

Entry: `examples::early_load_race::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Driver load-order audit — Non-AC memrw with load_order < ac_driver_load_order is a scar.
2. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `early_memrw`
- inspects drivers/devices

Win conditions for this pair:
- detected := `early_memrw`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (Driver.signer and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 46_early_load_race
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `early_load_race/red_example.cpp` — full red multi-step
- `early_load_race/blue_example.cpp` — full blue multi-reason
- `early_load_race/pair.cpp` — StrategyEntry wiring + narrator
