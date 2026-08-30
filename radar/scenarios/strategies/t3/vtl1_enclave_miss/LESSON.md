# 95_vtl1_enclave_miss — VTL1 enclave missing

Family: Structural. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Clear expected VTL1 enclave (vtl1_enclave_present=false)

Blue: Ranked deny if !vtl1_enclave_present

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::vtl1_enclave_miss::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Drop VTL1 enclave + personal HV path — Enclave miss + trust degrade + own VMX root; attest fails optional.
2. Step 1: residual — expected enclave absent (default present on hardened hosts).
3. Step 2: trust degrade (VBS/HVCI off so personal HV can start).
4. Step 3: personal HV delivery surface.
5. Step 4: optional attestation fail (secure-world / measured path broken).
6. Step 5: thin bridge scar for multi-surface correlation.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- vtl1_enclave_present — World.vtl1_enclave_present = false
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- trust.attestation_valid — HostTrust.attestation_valid = false
- trust.attestation_pcr_ok — HostTrust.attestation_pcr_ok = false
- load_driver() — Load Driver into World.drivers (kernel image scar).
- Driver.byovd_known_bad — Driver.byovd_known_bad = false (not BYOVD denylist path)
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- spawn() — Spawn actor process on World process list.

Achieved when: `red.enclave_missing`

## BLUE

Entry: `examples::vtl1_enclave_miss::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: VTL1 enclave presence + HV/attest multi-sensor — Enclave miss alone is thin; correlate with personal HV / attest / policy.
2. inspects drivers/devices

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `enclave_missing`
- result field `personal_hv`
- result field `attest_fail`
- result field `bridge_hit`
- result field `policy_deny`
- inspects drivers/devices

Win conditions for this pair:
- detected := `enclave_missing &&
           (personal_hv || bridge_hit || attest_fail || policy_deny ||
            reasons.size() >= 2)`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 95_vtl1_enclave_miss
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `vtl1_enclave_miss/red_example.cpp` — full red multi-step
- `vtl1_enclave_miss/blue_example.cpp` — full blue multi-reason
- `vtl1_enclave_miss/pair.cpp` — StrategyEntry wiring + narrator
