# 68_dkom_hide — DKOM process hide

Family: Evasion. Tiers: T2. Area: t2. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Hide cheat from weak process enum; keep handle or reader_active

Blue: Strong list_processes / handle graph when weak list is empty

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::dkom_hide::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: DKOM process hide — Unlink cheat from weak process lists; keep handle or reader_active.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- Process.hidden_from_weak_enum — Process.hidden_from_weak_enum = true
- Process.reader_active — Process.reader_active = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.

Achieved when: `red.achieved()`

## BLUE

Entry: `examples::dkom_hide::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Weak process list — list_processes(true) is DKOM-blind; miss is expected under hide.
2. Narrator counter: Strong process list — list_processes(false) walks full process set (ground truth).
3. Narrator counter: Handle graph + reader_active — Foreign VM_READ / reader_active independent of process lists.
4. local `weak_sees` init=false
5. reads handle graph via handles_to()
6. enumerates processes via list_processes()
7. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `weak_missed`
- result field `strong_hit`
- result field `found_pid`
- result field `handle_hit`
- result field `reader_active_hit`
- local `weak_sees` init=false
- reads handle graph via handles_to()
- enumerates processes via list_processes()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `strong_hit || handle_hit || reader_active_hit`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (Process.hidden_from_weak_enum and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 68_dkom_hide
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `dkom_hide/red_example.cpp` — full red multi-step
- `dkom_hide/blue_example.cpp` — full blue multi-reason
- `dkom_hide/pair.cpp` — StrategyEntry wiring + narrator
