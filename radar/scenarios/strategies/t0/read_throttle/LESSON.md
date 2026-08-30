# 18_read_throttle — Sparse/throttled reads

Family: Evasion. Tiers: T0-T2. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Lower read rate and field set

Blue: Attribute reads to foreign principal, not volume alone

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::read_throttle::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Read throttling — 20–60 Hz, sparse fields — looks less like a scanner by volume.

Team / depth APIs used:

World scars and lab surfaces (from shipped red code):
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- read_mem() — ReadProcessMemory-style entity/bytes pull through handle.
- spawn() — Spawn actor process on World process list.

Achieved when: `ok_reads > 0 && count > 0 && !volume_suspicious`

## BLUE

Entry: `examples::read_throttle::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Volume-only heuristics — Easy false negatives.
2. Narrator counter: Identity of reader — Handle graph ignores rate.
3. local `noted_hz` init=-1
4. local `volume_flag` init=noted_hz > 500
5. local `foreign_readers` init=0
6. reads handle graph via handles_to()
7. filters AccessMask::VmRead handles

Team / depth sensors:
- `depth::MultiSampleHandleDetector`
  - push multiple sample(w, i) then evaluate: composed_hit, race_detected, continuous_hit.
  - Not a single World bool — composition across samples + lineage + continuous truth.
- call `det.push()`
- call `det.sample()`
- call `det.evaluate()`

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `noted_hz` init=-1
- local `volume_flag` init=noted_hz > 500
- local `foreign_readers` init=0
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `foreign_readers > 0 || (ms.continuous_hit || ms.composed_hit)`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (open_process() AccessMask::VmRead and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 18_read_throttle
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0-T2
```

Open the pair sources beside this lesson:

- `read_throttle/red_example.cpp` — full red multi-step
- `read_throttle/blue_example.cpp` — full blue multi-reason
- `read_throttle/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/handle_multisample.hpp`
