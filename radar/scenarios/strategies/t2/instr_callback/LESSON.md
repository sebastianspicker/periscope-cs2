# 92_instr_callback — Instrumentation callback residual

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Kernel reader + InstrCallback residual + stack spoof

Blue: KernelAc + InstrCallback integrity correlation

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::instr_callback::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: InstrCallback + kernel reader — Install InstrumentationCallback residual while reading via IOCTL 

Team / depth APIs used:
- `t2_red::KernelRadar`
- call `radar.run_full_loop()`
- call `radar.has_game_handle()`

Expanded team path (what the wrapper actually does on sim::World):
- Bring up a kernel path (BYOVD / IOCTL / physmem depending on KernelPath).
- Load or open a lab driver/device that provides mem RW without a game usermode handle when the path succeeds.
- run_full_loop: entities_ok + brought_up + path-specific scars (byovd_known_bad, device.mem_rw_ioctl, no_game_handle).

World scars and lab surfaces (from shipped red code):
- instrumentation_callback — World.instrumentation_callback = true
- stack_spoof_on_read — World.stack_spoof_on_read = true
- Driver.byovd_known_bad and/or Device.mem_rw_ioctl.
- Often no usermode VM_READ on game (no_game_handle path).
- Callback / allowlist surfaces when path strips or races AC.

Achieved when: `red.achieved()`

## BLUE

Entry: `examples::instr_callback::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: InstrCallback integrity + KernelAc — Correlate InstrCallback residual with kernel memrw surface.
2. [t2_blue::KernelAc] full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
3. [t2_blue::KernelAc] mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.

Team / depth sensors:
- `t2_blue::KernelAc`
  - full_scan: BYOVD blocklist hash, suspicious mem-RW devices, callback integrity, driver allowlist.
  - mitigate() can flip ranked_access_denied / byovd_policy_block when policy wins without a classic detect.
- `ac::MemoryTelemetrySink`
- call `kac.full_scan()`

Multi-reason / result fields and sensors:
- result field `instrumentation_callback`
- result field `stack_spoof`
- result field `kernel_surface`
- result field `kernel_ac_hit`
- result field `risk`

Win conditions for this pair:
- detected := `instrumentation_callback && (kernel_surface || kernel_ac_hit)`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Kernel paths trade usermode handles for driver/device/callback scars. Blue wins on blocklist + integrity + ranked policy, not on RPM alone.

## Run

```bash
./build/strategy_lab run 92_instr_callback
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `instr_callback/red_example.cpp` — full red multi-step
- `instr_callback/blue_example.cpp` — full blue multi-reason
- `instr_callback/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `t2/red/kernel_radar.hpp`
- `t2/blue/kernel_ac.hpp`
