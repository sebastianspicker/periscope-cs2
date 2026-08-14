# 34_scm_service — SCM kernel-driver service

Family: Delivery. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Memrw registered as kernel-driver SCM service

Blue: Inventory kernel services; flag non-AC owners

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::scm_service::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: SCM kernel service — Register memrw as a kernel-driver service; load image + device.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.provides_mem_rw — Driver.provides_mem_rw = true (mem R/W kernel path)
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- Device.mem_rw_ioctl — Device.mem_rw_ioctl = true (IOCTL mem path)
- add_service() — SCM ServiceEvent registration.
- ServiceEvent.kernel_driver — ServiceEvent.kernel_driver = true

Achieved when: `loaded := true` (pair maps red_achieved := `red.loaded && red.service_added`)

## BLUE

Entry: `examples::scm_service::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: SCM service inventory — Kernel-driver services must be AC-owned or explainable.
2. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `rogue_kernel_service`
- inspects drivers/devices

Win conditions for this pair:
- detected := `rogue_kernel_service`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (load_driver() and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 34_scm_service
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `scm_service/red_example.cpp` — full red multi-step
- `scm_service/blue_example.cpp` — full blue multi-reason
- `scm_service/pair.cpp` — StrategyEntry wiring + narrator
