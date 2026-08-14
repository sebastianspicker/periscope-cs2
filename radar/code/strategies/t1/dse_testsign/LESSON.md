# 67_dse_testsign — DSE / test-signing posture

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: dse_enforced=false, test_signing=true

Blue: Ranked posture deny if !dse_enforced || test_signing

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::dse_testsign::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: DSE / test-signing off — Clear DSE enforcement; enable test-signing host posture.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.dse_enforced — HostTrust.dse_enforced = false
- trust.test_signing — HostTrust.test_signing = true

Achieved when: `out.dse_off && out.test_signing`

## BLUE

Entry: `examples::dse_testsign::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Ranked DSE posture — Deny ranked if !dse_enforced || test_signing.
2. Multi-reason scan over World scars (see sensors list below).

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `dse_off`
- result field `test_signing`
- result field `policy_deny`
- result field `detected`
- result field `mitigated`

Win conditions for this pair:
- detected := `out.policy_deny`
- mitigated := `out.policy_deny`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (trust.dse_enforced and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 67_dse_testsign
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `dse_testsign/red_example.cpp` — full red multi-step
- `dse_testsign/blue_example.cpp` — full blue multi-reason
- `dse_testsign/pair.cpp` — StrategyEntry wiring + narrator
