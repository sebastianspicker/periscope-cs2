# 58_elam_bypass — ELAM / Secure Launch bypass

Family: Evasion. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Clear ELAM+Secure Launch; plant early non-AC boot driver

Blue: Ranked deny if !elam||!secure_launch; early boot audit

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::elam_bypass::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Bypass ELAM / Secure Launch + personal HV — Disable early anti-malware + measured launch; early driver; then HV.
2. Step 1: platform early-trust scars — ELAM and Secure Launch off.
3. Step 2: early non-AC boot driver — races before AC (low load_order).
4. Step 3: free VMX after early trust break; personal HV.
5. Step 4: thin bridge scar (delivery under broken early trust).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.elam_enabled — HostTrust.elam_enabled = false
- trust.secure_launch — HostTrust.secure_launch = false
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- Driver.signer — Driver.signer = "unknown"
- Driver.boot_start — Driver.boot_start = true
- Driver.byovd_known_bad — Driver.byovd_known_bad = false
- Driver.is_ac — Driver.is_ac = false
- Driver.is_bridge — Driver.is_bridge = false
- Driver.provides_mem_rw — Driver.provides_mem_rw = false
- Driver.load_order — Driver.load_order = r.load_order
- load_driver() — Load Driver into World.drivers (kernel image scar).
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.elam_off && red.secure_launch_off && red.early_driver_loaded`

## BLUE

Entry: `examples::elam_bypass::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: ELAM / Secure Launch + HV multi-sensor — Ranked requires ELAM+SL; flag early boot drivers + personal HV path.
2. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `elam_missing`
- result field `secure_launch_missing`
- result field `early_boot_hit`
- result field `bridge_hit`
- result field `personal_hv`
- result field `policy_deny`
- result field `residual`
- inspects drivers/devices

Win conditions for this pair:
- detected := `blue.detected()`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 58_elam_bypass
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `elam_bypass/red_example.cpp` — full red multi-step
- `elam_bypass/blue_example.cpp` — full blue multi-reason
- `elam_bypass/pair.cpp` — StrategyEntry wiring + narrator
