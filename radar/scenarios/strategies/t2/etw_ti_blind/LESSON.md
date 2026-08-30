# 93_etw_ti_blind — ETW Threat Intelligence blind

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Kernel IOCTL radar + darken ETW/ETW-TI while no game handle

Blue: KernelAc + TI integrity correlation

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::etw_ti_blind::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: ETW-TI blind + kernel reader — Dark ETW-TI (and often ETW) so TI syscall/process feeds go silent, 
2. Step 1: no-handle kernel channel (T2 delivery).
3. Step 2: darken telemetry pipelines (classic ETW + TI residual).

Team / depth APIs used:
- `t2_red::KernelRadar`
- call `radar.run_full_loop()`
- call `radar.has_game_handle()`

Expanded team path (what the wrapper actually does on sim::World):
- Bring up a kernel path (BYOVD / IOCTL / physmem depending on KernelPath).
- Load or open a lab driver/device that provides mem RW without a game usermode handle when the path succeeds.
- run_full_loop: entities_ok + brought_up + path-specific scars (byovd_known_bad, device.mem_rw_ioctl, no_game_handle).

World scars and lab surfaces (from shipped red code):
- etw_enabled — World.etw_enabled = false
- etw_ti_blind — World.etw_ti_blind = true
- Driver.byovd_known_bad and/or Device.mem_rw_ioctl.
- Often no usermode VM_READ on game (no_game_handle path).
- Callback / allowlist surfaces when path strips or races AC.

Achieved when: `red.achieved()`

## BLUE

Entry: `examples::etw_ti_blind::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: ETW/TI integrity + KernelAc — TI dark alone is weak; correlate with memrw device/driver surface.
2. [t2_blue::KernelAc] full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
3. [t2_blue::KernelAc] mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.

Team / depth sensors:
- `t2_blue::KernelAc`
  - full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
  - mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.
- `ac::MemoryTelemetrySink`
- call `kac.full_scan()`

Multi-reason / result fields and sensors:
- result field `etw_ti_blind`
- result field `etw_disabled`
- result field `kernel_ac_hit`
- result field `kernel_surface`
- result field `risk`

Win conditions for this pair:
- detected := `etw_ti_blind && (kernel_surface || kernel_ac_hit)`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Kernel paths trade usermode handles for driver/device/callback scars. Blue wins on blocklist + integrity + ranked policy, not on RPM alone.

## Run

```bash
./build/strategy_lab run 93_etw_ti_blind
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `etw_ti_blind/red_example.cpp` — full red multi-step
- `etw_ti_blind/blue_example.cpp` — full blue multi-reason
- `etw_ti_blind/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `t2/red/kernel_radar.hpp`
- `t2/blue/kernel_ac.hpp`
