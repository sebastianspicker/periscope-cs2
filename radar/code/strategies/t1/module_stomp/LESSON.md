# 54_module_stomp — Module stomping

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Stomp client.dll .text (text_hash=stomped) inside game process

Blue: Detect known modules (client.dll/game.exe) with text_hash != clean

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::module_stomp::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Module stomp — Overwrite client.dll .text in-place (text_hash=stomped).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- client.dll.text_hash — client.dll Module.text_hash = "stomped"
- text_hash="stomped" — Module.text_hash set to stomped (integrity scar)

Achieved when: `true after planting: stomped=true, Module.text_hash="stomped", client.dll.text_hash="stomped" — module_stomp red: client.dll text_hash=stomped`

## BLUE

Entry: `examples::module_stomp::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Known-module text integrity — Hash client.dll/game.exe; flag text_hash != clean.
2. local `dirty` init=0
3. checks Module.text_hash integrity

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `dirty` init=0
- checks Module.text_hash integrity

Win conditions for this pair:
- detected := `dirty > 0`
- mitigated := `dirty > 0`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (client.dll.text_hash and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 54_module_stomp
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `module_stomp/red_example.cpp` — full red multi-step
- `module_stomp/blue_example.cpp` — full blue multi-reason
- `module_stomp/pair.cpp` — StrategyEntry wiring + narrator
