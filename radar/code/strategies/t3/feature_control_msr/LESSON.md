# 83_feature_control_msr — Feature-control MSR spoof

Family: Evasion. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Spoof IA32_FEATURE_CONTROL; optional personal HV

Blue: Detect feature_control_spoofed; ranked policy deny

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::feature_control_msr::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Feature-control MSR spoof + personal HV path — Spoof IA32_FEATURE_CONTROL; trust off; own VMX root; bridge + timing.
2. Step 1: residual — feature-control MSR presented as locked/clean to guests.
3. Step 2: free VMX for personal HV.
4. Step 3: personal HV delivery surface.
5. Step 4: thin bridge scar.
6. Step 5: optional timing spoof (CPUID latency multi-leaf inconsistency).
7. Step 6: optional entity read via HV.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- feature_control_spoofed — World.feature_control_spoofed = true
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- trust.timing_spoofed — HostTrust.timing_spoofed = true
- trust.cpuid_latency_ns — HostTrust.cpuid_latency_ns = w.trust.baseline_latency_ns * 0.4
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.feature_control_spoofed`

## BLUE

Entry: `examples::feature_control_msr::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Feature-control MSR + HV/bridge multi-sensor — MSR spoof alone is thin; correlate with personal HV / bridge / timing.
2. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `feature_control_spoofed`
- result field `personal_hv`
- result field `bridge_hit`
- result field `timing_hit`
- result field `policy_deny`
- inspects drivers/devices

Win conditions for this pair:
- detected := `feature_control_spoofed &&
           (personal_hv || bridge_hit || timing_hit || policy_deny ||
            reasons.size() >= 2)`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 83_feature_control_msr
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `feature_control_msr/red_example.cpp` — full red multi-step
- `feature_control_msr/blue_example.cpp` — full blue multi-reason
- `feature_control_msr/pair.cpp` — StrategyEntry wiring + narrator
