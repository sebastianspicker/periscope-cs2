# 78_thread_hide_dbg — ThreadHideFromDebugger / PEB spoof

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: thread_hide_from_debugger=true, peb_being_debugged_spoofed=true; may open handle

Blue: Detect either thread_hide_from_debugger or peb_being_debugged_spoofed

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::thread_hide_dbg::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: ThreadHide + PEB anti-debug — Set thread_hide_from_debugger + peb_being_debugged_spoofed; may open handle.
2. Optional co-scar: soft attach still often opens a game handle (T1 path).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- thread_hide_from_debugger — World.thread_hide_from_debugger = true
- peb_being_debugged_spoofed — World.peb_being_debugged_spoofed = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.

Achieved when: `out.thread_hidden && out.peb_spoofed`

## BLUE

Entry: `examples::thread_hide_dbg::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Anti-debug flags — Detect thread_hide_from_debugger OR peb_being_debugged_spoofed.
2. Sensor A: ThreadHideFromDebugger class scar (kernel/debug port visibility).
3. Sensor B: PEB BeingDebugged / related usermode anti-debug spoof.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `thread_hide_hit`
- result field `peb_spoof_hit`
- result field `detected`
- result field `mitigated`

Win conditions for this pair:
- detected := `out.thread_hide_hit || out.peb_spoof_hit`
- mitigated := `out.detected`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (thread_hide_from_debugger and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 78_thread_hide_dbg
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `thread_hide_dbg/red_example.cpp` — full red multi-step
- `thread_hide_dbg/blue_example.cpp` — full blue multi-reason
- `thread_hide_dbg/pair.cpp` — StrategyEntry wiring + narrator
