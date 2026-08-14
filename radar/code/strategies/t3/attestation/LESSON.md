# 37_attestation — Platform attestation gate

Family: Structural. Tiers: T3. Area: t3. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Break quote signature / PCR match

Blue: Ranked deny if attestation invalid or PCR not known-good

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::attestation::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Break attestation quote / PCR — Invalidate signature and PCR match so ranked gate cannot trust host.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.attestation_valid — HostTrust.attestation_valid = false
- trust.attestation_pcr_ok — HostTrust.attestation_pcr_ok = false
- trust.vbs — HostTrust.vbs = false

Achieved when: `red.quote_broken || red.pcr_broken`

## BLUE

Entry: `examples::attestation::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Attestation ranked gate — Hard deny when quote signature or PCR is not known-good.
2. Multi-reason scan over World scars (see sensors list below).

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `policy_deny`

Win conditions for this pair:
- detected := `false`
- mitigated := `blue.mitigated()`
- Pass narrative: mitigation-only (fog/policy/structural) — detect may stay false; that is still a blue win.

## Takeaway

Blue can constrain or fog the advantage without a classic client scar detect. Check mitigated and server-side flags; structural lessons still count as blue wins.

## Run

```bash
./build/strategy_lab run 37_attestation
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `attestation/red_example.cpp` — full red multi-step
- `attestation/blue_example.cpp` — full blue multi-reason
- `attestation/pair.cpp` — StrategyEntry wiring + narrator
