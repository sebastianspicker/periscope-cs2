# 38_timing_spoof — HV timing spoof

Family: Evasion. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Personal HV; fake CPUID latency near baseline

Blue: Multi-invariant: vendor / spoof flag / latency×vendor

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::timing_spoof::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Spoof HV timing — Personal HV with foreign vendor; fake CPUID latency near baseline.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.vbs — HostTrust.vbs = false
- trust.hvci — HostTrust.hvci = false
- trust.timing_spoofed — HostTrust.timing_spoofed = true
- trust.cpuid_latency_ns — HostTrust.cpuid_latency_ns = w.trust.baseline_latency_ns * 1.1
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).

Achieved when: `red.hv_started && red.timing_spoofed`

## BLUE

Entry: `examples::timing_spoof::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Multi-invariant HV timing + trust timeline — Vendor, spoof flag, latency×vendor, and TrustAggregator over ticks.
2. local `latency_hi` init=t.baseline_latency_ns > 0 &&
      t.cpuid_latency_ns > t.baseline_latency_ns * 5.0
3. local `vendor_mismatch` init=!t.hv_vendor.empty() && t.hv_vendor != t.platform_hv_vendor &&
      !vendor_trusted(t.hv_vendor)

Team / depth sensors:
- `depth::TrustAggregator`

Multi-reason / result fields and sensors:
- result field `vendor_hit`
- result field `spoof_flag`
- result field `latency_vendor`
- local `latency_hi` init=t.baseline_latency_ns > 0 &&
      t.cpuid_latency_ns > t.baseline_latency_ns * 5.0
- local `vendor_mismatch` init=!t.hv_vendor.empty() && t.hv_vendor != t.platform_hv_vendor &&
      !vendor_trusted(t.hv_vendor)

Win conditions for this pair:
- detected := `vendor_hit || spoof_flag || latency_vendor`
- mitigated := `blue.mitigated()`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Hypervisor path hides classic usermode scars; blue answers with HostTrust, attestation, and dual-view (secure-kernel vs guest) multi-reason probes.

## Run

```bash
./build/strategy_lab run 38_timing_spoof
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `timing_spoof/red_example.cpp` — full red multi-step
- `timing_spoof/blue_example.cpp` — full blue multi-reason
- `timing_spoof/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/trust_aggregator.hpp`
