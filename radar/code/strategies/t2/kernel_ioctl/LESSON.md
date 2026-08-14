# 03_kernel_ioctl — Kernel IOCTL read

Family: Delivery. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Driver R/W without usermode game handle

Blue: Image load + device/IOCTL correlation

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::kernel_ioctl::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Custom memrw driver + IOCTL — Private .sys + device; no OpenProcess on game.

Team / depth APIs used:
- `t2_red::KernelRadar`
- call `radar.run_full_loop()`
- call `radar.ui_pid()`
- call `radar.game_pid()`
- call `radar.device()`
- call `radar.has_game_handle()`

Expanded team path (what the wrapper actually does on sim::World):
- Bring up a kernel path (BYOVD / IOCTL / physmem depending on KernelPath).
- Load or open a lab driver/device that provides mem RW without a game usermode handle when the path succeeds.
- run_full_loop: entities_ok + brought_up + path-specific scars (byovd_known_bad, device.mem_rw_ioctl, no_game_handle).

World scars and lab surfaces (from shipped red code):
- Driver.byovd_known_bad and/or Device.mem_rw_ioctl.
- Often no usermode VM_READ on game (no_game_handle path).
- Callback / allowlist surfaces when path strips or races AC.

Achieved when: `red.read_ok && !red.has_game_handle`

## BLUE

Entry: `examples::kernel_ioctl::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Handle scan — Expect clean for pure T2 — empty handle graph is not a miss.
2. Narrator counter: KernelAc full_scan — Unknown memrw driver and device are the scar.
3. [t2_blue::KernelAc] full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
4. [t2_blue::KernelAc] mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.
5. reads handle graph via handles_to()
6. filters AccessMask::VmRead handles

Team / depth sensors:
- `t2_blue::KernelAc`
  - full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
  - mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.
- `ac::MemoryTelemetrySink`
- call `kac.full_scan()`

Multi-reason / result fields and sensors:
- result field `handle_hit`
- result field `memrw_driver`
- result field `memrw_device`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `memrw_driver || memrw_device`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Kernel paths trade usermode handles for driver/device/callback scars. Blue wins on blocklist + integrity + ranked policy, not on RPM alone.

## Run

```bash
./build/strategy_lab run 03_kernel_ioctl
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `kernel_ioctl/red_example.cpp` — full red multi-step
- `kernel_ioctl/blue_example.cpp` — full blue multi-reason
- `kernel_ioctl/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `t2/red/kernel_radar.hpp`
- `t2/blue/kernel_ac.hpp`
