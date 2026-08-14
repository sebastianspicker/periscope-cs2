# 36_nested_hv — Nested personal hypervisor

Family: Delivery. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Platform HV + disable VBS; personal ACLABHV

Blue: Detect personal under Microsoft-only platform; policy fail

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::nested_hv::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Nested personal HV under platform + bridge — Keep platform HV bit, kill VBS/HVCI, own VMX root with ACLABHV + bridge.
2. Step 1: platform expects Microsoft Hv (Hyper-V / VBS host still "present").
3. Step 2: nested personal HV needs free VMX — clear integrity.
4. Step 3: personal HV with foreign vendor.
5. Step 4: thin bridge scar (delivery under nested HV).
6. Step 5: optional entity read via HV.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.platform_hv_active — HostTrust.platform_hv_active = true
- trust.platform_hv_vendor — HostTrust.platform_hv_vendor = "Microsoft Hv"
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.personal_hv`

## BLUE

Entry: `examples::nested_hv::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Nested HV probe + bridge multi-sensor — Platform may run Microsoft Hv; personal foreign HV + bridge is nested scar.
2. local `platform_expects_ms` init=t.platform_hv_active &&
      (t.platform_hv_vendor == "Microsoft Hv" || t.platform_hv_vendor.empty())
3. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `personal_hv`
- result field `policy_fail`
- result field `nested_detect`
- result field `bridge_hit`
- local `platform_expects_ms` init=t.platform_hv_active &&
      (t.platform_hv_vendor == "Microsoft Hv" || t.platform_hv_vendor.empty())
- inspects drivers/devices

Win conditions for this pair:
- detected := `nested_detect &&
           (bridge_hit || policy_fail || personal_hv || reasons.size() >= 2)`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 36_nested_hv
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `nested_hv/red_example.cpp` — full red multi-step
- `nested_hv/blue_example.cpp` — full blue multi-reason
- `nested_hv/pair.cpp` — StrategyEntry wiring + narrator
