# 53_thread_hijack — Thread hijack / APC scar

Family: Evasion. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Spawn helper; mark game thread_hijacked + has_foreign_thread (optional handle)

Blue: Any is_game with thread_hijacked || has_foreign_thread (non-AC context)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::thread_hijack::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Thread hijack / APC — Spawn helper; set game thread_hijacked + has_foreign_thread (optional handle).
2. Optional handle scar (thread ops need a process/thread handle path).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- Process.thread_hijacked — Process.thread_hijacked = true
- Process.has_foreign_thread — Process.has_foreign_thread = true
- open_process() AccessMask::VmWrite — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmWrite
- spawn() — Spawn actor process on World process list.

Achieved when: `g->thread_hijacked && g->has_foreign_thread`

## BLUE

Entry: `examples::thread_hijack::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Foreign thread / hijack scars — Any is_game process with thread_hijacked || has_foreign_thread.
2. local `hits` init=0
3. checks Process inject/hijack/hollow scars

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `hits` init=0
- checks Process inject/hijack/hollow scars

Win conditions for this pair:
- detected := `hits > 0`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (Process.thread_hijacked and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 53_thread_hijack
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `thread_hijack/red_example.cpp` — full red multi-step
- `thread_hijack/blue_example.cpp` — full blue multi-reason
- `thread_hijack/pair.cpp` — StrategyEntry wiring + narrator
