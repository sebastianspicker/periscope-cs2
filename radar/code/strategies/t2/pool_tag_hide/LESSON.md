# 80_pool_tag_hide — Pool-tag hide residual

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Memrw driver + pool-tag hide while IOCTL-reading entities

Blue: KernelAc + pool inventory vs kernel surface

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::pool_tag_hide::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Pool-tag hide + kernel reader — Rewrite/hide pool tags on cheat-driver allocations while reading 

Team / depth APIs used:
- `t2_red::KernelRadar`
- call `radar.run_full_loop()`
- call `radar.driver_sha()`

Expanded team path (what the wrapper actually does on sim::World):
- Bring up a kernel path (BYOVD / IOCTL / physmem depending on KernelPath).
- Load or open a lab driver/device that provides mem RW without a game usermode handle when the path succeeds.
- run_full_loop: entities_ok + brought_up + path-specific scars (byovd_known_bad, device.mem_rw_ioctl, no_game_handle).

World scars and lab surfaces (from shipped red code):
- pool_tag_anomaly — World.pool_tag_anomaly = true
- Driver.byovd_known_bad and/or Device.mem_rw_ioctl.
- Often no usermode VM_READ on game (no_game_handle path).
- Callback / allowlist surfaces when path strips or races AC.

Achieved when: `red.achieved()`

## BLUE

Entry: `examples::pool_tag_hide::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Pool inventory + KernelAc — pool_tag_anomaly alone is weak; require concurrent kernel surface.
2. [t2_blue::KernelAc] full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
3. [t2_blue::KernelAc] mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.
4. local `memrw_drivers` init=0
5. inspects drivers/devices

Team / depth sensors:
- `t2_blue::KernelAc`
  - full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
  - mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.
- `ac::MemoryTelemetrySink`
- call `kac.full_scan()`

Multi-reason / result fields and sensors:
- result field `pool_tag_anomaly`
- result field `kernel_surface`
- result field `kernel_ac_hit`
- result field `risk`
- local `memrw_drivers` init=0
- inspects drivers/devices

Win conditions for this pair:
- detected := `pool_tag_anomaly && (kernel_surface || kernel_ac_hit)`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Kernel paths trade usermode handles for driver/device/callback scars. Blue wins on blocklist + integrity + ranked policy, not on RPM alone.

## Run

```bash
./build/strategy_lab run 80_pool_tag_hide
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `pool_tag_hide/red_example.cpp` — full red multi-step
- `pool_tag_hide/blue_example.cpp` — full blue multi-reason
- `pool_tag_hide/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `t2/red/kernel_radar.hpp`
- `t2/blue/kernel_ac.hpp`
