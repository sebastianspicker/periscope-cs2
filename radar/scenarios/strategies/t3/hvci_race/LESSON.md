# 71_hvci_race — HVCI race / continuous policy

Family: Evasion. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Race or disable HVCI (hvci_enabled=false); optional personal HV

Blue: Continuous policy: !hvci_enabled → ranked deny / detect

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::hvci_race::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: HVCI race + personal HV delivery — Clear hvci_enabled; free VBS; start personal HV + bridge scar.
2. Step 1: continuous policy scar — HVCI no longer enforced (race/disable).
3. Step 2: clear platform integrity for personal HV path.
4. Step 3: personal HV delivery surface.
5. Step 4: thin bridge scar.
6. Step 5: optional entity read via HV.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.hvci_enabled — HostTrust.hvci_enabled = false
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.hvci_off`

## BLUE

Entry: `examples::hvci_race::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Continuous HVCI + HV/bridge multi-sensor — !hvci_enabled residual; re-check and correlate with personal HV / bridge.
2. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `hvci_missing`
- result field `personal_hv`
- result field `bridge_hit`
- result field `policy_deny`
- inspects drivers/devices

Win conditions for this pair:
- detected := `hvci_missing &&
           (personal_hv || bridge_hit || policy_deny || reasons.size() >= 2)`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 71_hvci_race
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `hvci_race/red_example.cpp` — full red multi-step
- `hvci_race/blue_example.cpp` — full blue multi-reason
- `hvci_race/pair.cpp` — StrategyEntry wiring + narrator
