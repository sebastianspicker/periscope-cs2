# 05_hypervisor — Personal hypervisor read

Family: Delivery. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: HV introspection + bridge UI

Blue: VBS/HVCI policy, HV probes, bridge driver intel

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::hypervisor::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Personal hypervisor — Disable VBS/HVCI, own VMX root (sim), GPA walk; thin bridge to UI.
2. Optional GPA/VA read via simulated HV path (no real VMX).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.boot_start — Driver.boot_start = true (early/boot load)
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- Device.mem_rw_ioctl — Device.mem_rw_ioctl = true (IOCTL mem path)
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.hv_started`

## BLUE

Entry: `examples::hypervisor::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Multi-invariant trust aggregate — Timeline of VBS/HVCI/attest/EPT/timing/bridge — not one flag.
2. Multi-sample trust aggregation over time (shipped depth API).
3. reads HostTrust platform fields
4. inspects drivers/devices

Team / depth sensors:
- `depth::TrustAggregator`

Multi-reason / result fields and sensors:
- result field `policy_fail`
- result field `hv_probe`
- result field `bridge_hit`
- reads HostTrust platform fields
- inspects drivers/devices

Win conditions for this pair:
- detected := `hv_probe || bridge_hit`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 05_hypervisor
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `hypervisor/red_example.cpp` — full red multi-step
- `hypervisor/blue_example.cpp` — full blue multi-reason
- `hypervisor/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/trust_aggregator.hpp`
