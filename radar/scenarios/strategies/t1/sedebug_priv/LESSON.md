# 90_sedebug_priv — SeDebugPrivilege enable

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: sedebug_privilege=true; may open handle

Blue: Detect sedebug_privilege

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::sedebug_priv::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: SeDebugPrivilege — Set sedebug_privilege=true; may open game handle.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- sedebug_privilege — World.sedebug_privilege = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.

Achieved when: `out.sedebug`

## BLUE

Entry: `examples::sedebug_priv::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Token privilege inventory — Detect sedebug_privilege.
2. Sensor: token privilege inventory / SeDebugPrivilege enabled on session.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `sedebug_hit`
- result field `detected`
- result field `mitigated`

Win conditions for this pair:
- detected := `out.sedebug_hit`
- mitigated := `out.detected`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (sedebug_privilege and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 90_sedebug_priv
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `sedebug_priv/red_example.cpp` — full red multi-step
- `sedebug_priv/blue_example.cpp` — full blue multi-reason
- `sedebug_priv/pair.cpp` — StrategyEntry wiring + narrator
