# 70_secure_kernel_view — Secure Kernel dual-view

Family: Evasion. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Guest AC self-hash clean while Secure Kernel sees tamper

Blue: Detect secure_kernel_view_dirty even if guest clean; ranked deny

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::secure_kernel_view::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Secure Kernel dual-view (guest clean / SK dirty) — Personal HV or VBS off; guest self-hash clean while SK sees tamper.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- trust.guest_ac_view_clean — HostTrust.guest_ac_view_clean = true
- trust.secure_kernel_view_dirty — HostTrust.secure_kernel_view_dirty = true
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).

Achieved when: `red.guest_clean && red.sk_view_dirty`

## BLUE

Entry: `examples::secure_kernel_view::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Secure Kernel dual-view integrity — Guest self-hash can lie; detect secure_kernel_view_dirty; deny.
2. Multi-reason scan over World scars (see sensors list below).

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `guest_clean`
- result field `personal_hv`
- result field `sk_view_dirty`
- result field `dual_view_hit`
- result field `policy_deny`

Win conditions for this pair:
- detected := `sk_view_dirty || dual_view_hit`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Kernel paths trade usermode handles for driver/device/callback scars. Blue wins on blocklist + integrity + ranked policy, not on RPM alone.

## Run

```bash
./build/strategy_lab run 70_secure_kernel_view
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `secure_kernel_view/red_example.cpp` — full red multi-step
- `secure_kernel_view/blue_example.cpp` — full blue multi-reason
- `secure_kernel_view/pair.cpp` — StrategyEntry wiring + narrator
