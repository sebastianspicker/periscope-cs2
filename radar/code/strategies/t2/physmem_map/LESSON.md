# 69_physmem_map — Physmem map path

Family: Delivery. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: physmem_device_open + memrw driver; no usermode game handle

Blue: Detect physmem open or memrw without foreign VM_READ

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::physmem_map::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Physmem map path — MmMapIoSpace-class device + memrw driver; no game VM_READ handle.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- physmem_device_open — World.physmem_device_open = true
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.provides_mem_rw — Driver.provides_mem_rw = true (mem R/W kernel path)
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- Device.mem_rw_ioctl — Device.mem_rw_ioctl = true (IOCTL mem path)
- spawn() — Spawn actor process on World process list.

Achieved when: `red.achieved()`

## BLUE

Entry: `examples::physmem_map::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Physmem device watch — World::physmem_device_open is an MmMapIoSpace-class scar.
2. Narrator counter: Memrw vs handle graph — Non-AC provides_mem_rw with empty foreign VM_READ is a kernel path.
3. reads handle graph via handles_to()
4. inspects drivers/devices
5. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `physmem_device_open`
- result field `memrw_driver`
- result field `foreign_vm_read`
- result field `memrw_without_handle`
- reads handle graph via handles_to()
- inspects drivers/devices
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `physmem_device_open || memrw_without_handle`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (physmem_device_open and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 69_physmem_map
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `physmem_map/red_example.cpp` — full red multi-step
- `physmem_map/blue_example.cpp` — full blue multi-reason
- `physmem_map/pair.cpp` — StrategyEntry wiring + narrator
