# 27_boot_trust — Boot & platform trust

Family: Structural. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Disable SB/VBS; early loaders

Blue: Ranked attestation + boot driver allowlist

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::boot_trust::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Disable boot trust + personal HV path — Clear SB/VBS/HVCI; early boot loader; own VMX root + bridge.
2. Step 1: clear platform trust bits.
3. Step 2: unknown early loader — boot_start, not AC, not on an allowlist.
4. Step 3: personal HV after trust field is cleared.
5. Step 4: thin bridge scar.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.secure_boot — HostTrust.secure_boot = false
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.boot_start — Driver.boot_start = true (early/boot load)
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.trust_cleared`

## BLUE

Entry: `examples::boot_trust::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Boot/trust gate + HV multi-sensor — Require Secure Boot + VBS/HVCI; inventory boot-start + personal HV.
2. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `policy_deny`
- result field `personal_hv`
- result field `unknown_boot`
- result field `bridge_hit`
- inspects drivers/devices

Win conditions for this pair:
- detected := `unknown_boot &&
           (personal_hv || bridge_hit || policy_deny || reasons.size() >= 2)`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 27_boot_trust
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `boot_trust/red_example.cpp` — full red multi-step
- `boot_trust/blue_example.cpp` — full blue multi-reason
- `boot_trust/pair.cpp` — StrategyEntry wiring + narrator
