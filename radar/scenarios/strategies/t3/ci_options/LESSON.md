# 82_ci_options — Code integrity options disabled

Family: Structural. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Disable CI options (ci_options_disabled=true)

Blue: Ranked deny if ci_options_disabled

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::ci_options::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: CI options off + personal HV delivery — Clear VBS/HVCI, disable CI options, own VMX root, bridge + hv_read.
2. Step 1: free VMX / integrity so personal HV and unsigned path open.
3. Step 2: residual scar — Code Integrity options flipped off (g_CiOptions-class).
4. Step 3: personal HV delivery surface.
5. Step 4: thin bridge scar (guest-visible is_bridge driver/device).
6. Step 5: optional entity read via HV path (correlate residual with delivery).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- ci_options_disabled — World.ci_options_disabled = true
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.ci_off`

## BLUE

Entry: `examples::ci_options::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: CI options + HV/bridge multi-sensor — ci_options_disabled alone is thin; correlate with personal HV / bridge.
2. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `ci_disabled`
- result field `personal_hv`
- result field `bridge_hit`
- result field `policy_deny`
- inspects drivers/devices

Win conditions for this pair:
- detected := `ci_disabled &&
           (personal_hv || bridge_hit || policy_deny || reasons.size() >= 2)`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 82_ci_options
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `ci_options/red_example.cpp` — full red multi-step
- `ci_options/blue_example.cpp` — full blue multi-reason
- `ci_options/pair.cpp` — StrategyEntry wiring + narrator
