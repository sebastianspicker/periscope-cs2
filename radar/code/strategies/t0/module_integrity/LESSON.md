# 21_module_integrity — Game module integrity

Family: Detection. Tiers: T0-T1 internal. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Patch/hooks game code

Blue: Continuous .text hashing (necessary, not sufficient)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::module_integrity::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Patch game .text (internal rage) — Classic hooks — high power, high integrity signal.
2. Narrator move: Contrast: external radar — Leaves .text clean — integrity alone is insufficient.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- client.dll.text_hash — client.dll Module.text_hash = "patched"
- text_hash="patched" — Module.text_hash set to patched (integrity scar)

Achieved when: `true after planting: patched=true, Module.text_hash="patched", client.dll.text_hash="patched" — module_integrity red: client.dll text_hash=patched`

## BLUE

Entry: `examples::module_integrity::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Continuous module integrity — Hash .text periodically, not only at boot.
2. Contrast: pure external RPM leaves hashes clean — integrity alone misses it.
3. local `dirty` init=0
4. local `foreign_handles` init=0
5. reads handle graph via handles_to()
6. checks Module.text_hash integrity
7. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `dirty` init=0
- local `foreign_handles` init=0
- reads handle graph via handles_to()
- checks Module.text_hash integrity
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `dirty > 0`
- mitigated := `dirty > 0`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (client.dll.text_hash and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 21_module_integrity
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0-T1
```

Open the pair sources beside this lesson:

- `module_integrity/red_example.cpp` — full red multi-step
- `module_integrity/blue_example.cpp` — full blue multi-reason
- `module_integrity/pair.cpp` — StrategyEntry wiring + narrator
