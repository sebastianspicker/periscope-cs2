# 94_infinity_hook — InfinityHook residual

Family: Evasion. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Leave infinity_hook_residual; optional personal HV

Blue: Detect infinity_hook_residual; optional ranked policy deny

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::infinity_hook::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: InfinityHook residual + personal HV path — Syscall/HAL residual + trust off + own VMX root + bridge scar.
2. Step 1: residual scar — InfinityHook-class residual present.
3. Step 2: free VMX for personal HV (trust off).
4. Step 3: personal HV delivery surface.
5. Step 4: thin bridge scar (guest-visible is_bridge).
6. Step 5: optional entity read via HV.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- infinity_hook_residual — World.infinity_hook_residual = true
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.infinity_hook_residual`

## BLUE

Entry: `examples::infinity_hook::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: InfinityHook residual + HV/bridge multi-sensor — Residual alone is weak; correlate with personal HV / bridge / trust.
2. reads HostTrust platform fields
3. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `infinity_hook_residual`
- result field `personal_hv`
- result field `bridge_hit`
- result field `policy_deny`
- reads HostTrust platform fields
- inspects drivers/devices

Win conditions for this pair:
- detected := `infinity_hook_residual &&
           (personal_hv || bridge_hit || policy_deny || reasons.size() >= 2)`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 94_infinity_hook
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `infinity_hook/red_example.cpp` — full red multi-step
- `infinity_hook/blue_example.cpp` — full blue multi-reason
- `infinity_hook/pair.cpp` — StrategyEntry wiring + narrator
