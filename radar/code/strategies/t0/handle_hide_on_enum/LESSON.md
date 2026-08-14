# 42_handle_hide_on_enum — Handle hide on enum

Family: Evasion. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: OpenProcess(VM_READ) then hide handle during AC enumeration

Blue: Single-sample may miss; count_hidden / continuous include_hidden detects

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::handle_hide_on_enum::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Hide handle on enum — OpenProcess(VM_READ), mark hidden_during_enum so a single sample misses.
2. Multi-step race: open → hide during AC sample → brief reopen → hide.

Team / depth APIs used:
- `depth::run_handle_race`

Expanded team path (what the wrapper actually does on sim::World):
- [depth::run_handle_race] Open VM_READ → mark hidden_during_enum → brief_reopen → optional parent lineage / reputation claim.
- [depth::run_handle_race] Returns opened, hid, read_ok for multi-step achieved.

World scars and lab surfaces (from shipped red code):
- Handle with VM_READ + hidden_during_enum + brief_reopen multi-step race.
- Optional parent lineage / looks_reputable on actor process.

Achieved when: `race.opened && race.hid && race.read_ok`

## BLUE

Entry: `examples::handle_hide_on_enum::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Continuous / hidden count — handles_to(false) may miss; count_hidden_handles_to or include_hidden.
2. Sample A: visible enum (may miss hidden)
3. Sample B: same world (truth/hidden already marked by red race)
4. Sample C: re-check continuous

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

Win conditions for this pair:
- detected := `ev.composed_hit || ev.race_detected || ev.continuous_hit`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Hide-on-enum races beat a single handle sample. Blue needs multi-sample composition (visible vs continuous/hidden) plus lineage — not one EnumHandles call.

## Run

```bash
./build/strategy_lab run 42_handle_hide_on_enum
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `handle_hide_on_enum/red_example.cpp` — full red multi-step
- `handle_hide_on_enum/blue_example.cpp` — full blue multi-reason
- `handle_hide_on_enum/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/handle_multisample.hpp`
