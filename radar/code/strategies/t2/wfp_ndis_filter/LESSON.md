# 81_wfp_ndis_filter — WFP/NDIS packet filter

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Kernel reader + WFP/NDIS filter + lag + SCM filter service

Blue: KernelAc + WFP/SCM/lag multi-signal detect

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::wfp_ndis_filter::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: WFP/NDIS filter + kernel radar — Install packet filter residual for lag/drop while reading entities 

Team / depth APIs used:
- `t2_red::KernelRadar`
- call `radar.run_full_loop()`

Expanded team path (what the wrapper actually does on sim::World):
- Bring up a kernel path (BYOVD / IOCTL / physmem depending on KernelPath).
- Load or open a lab driver/device that provides mem RW without a game usermode handle when the path succeeds.
- run_full_loop: entities_ok + brought_up + path-specific scars (byovd_known_bad, device.mem_rw_ioctl, no_game_handle).

World scars and lab surfaces (from shipped red code):
- wfp_ndis_filter — World.wfp_ndis_filter = true
- lag_switch_active — World.lag_switch_active = true
- add_service() — SCM ServiceEvent registration.
- Driver.byovd_known_bad and/or Device.mem_rw_ioctl.
- Often no usermode VM_READ on game (no_game_handle path).
- Callback / allowlist surfaces when path strips or races AC.

Achieved when: `red.achieved()`

## BLUE

Entry: `examples::wfp_ndis_filter::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: WFP/NDIS inventory + KernelAc — Correlate packet-filter residual with kernel reader + SCM.
2. [t2_blue::KernelAc] full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
3. [t2_blue::KernelAc] mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.

Team / depth sensors:
- `t2_blue::KernelAc`
  - full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
  - mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.
- `ac::MemoryTelemetrySink`
- call `kac.full_scan()`

Multi-reason / result fields and sensors:
- result field `wfp_ndis_filter`
- result field `lag_switch`
- result field `service_hit`
- result field `kernel_surface`
- result field `kernel_ac_hit`
- result field `risk`

Win conditions for this pair:
- detected := `wfp_ndis_filter &&
           (kernel_surface || service_hit || kernel_ac_hit || lag_switch)`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Kernel paths trade usermode handles for driver/device/callback scars. Blue wins on blocklist + integrity + ranked policy, not on RPM alone.

## Run

```bash
./build/strategy_lab run 81_wfp_ndis_filter
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `wfp_ndis_filter/red_example.cpp` — full red multi-step
- `wfp_ndis_filter/blue_example.cpp` — full blue multi-reason
- `wfp_ndis_filter/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `t2/red/kernel_radar.hpp`
- `t2/blue/kernel_ac.hpp`
