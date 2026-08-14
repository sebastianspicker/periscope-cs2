# 29_process_cooccurrence — Process co-occurrence radar

Family: Detection. Tiers: T0. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Co-run lab-radar/esp-named process with VM_READ on the live game

Blue: list_processes name heuristic AND/OR handle graph foreign VM_READ

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::process_cooccurrence::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Process co-occurrence — Spawn lab-radar alongside the game; OpenProcess(VM_READ).

Team / depth APIs used:

World scars and lab surfaces (from shipped red code):
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- read_mem() — ReadProcessMemory-style entity/bytes pull through handle.
- spawn() — Spawn actor process on World process list.

Achieved when: `count > 0`

## BLUE

Entry: `examples::process_cooccurrence::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Co-run + handle graph — Suspicious process name while game is live, and/or foreign VM_READ.
2. Sensor 1: process co-occurrence — suspicious name while game is live.
3. Sensor 2: handle graph — foreign VM_READ into the game (durable scar).
4. local `co_hits` init=0
5. local `foreign_vm_read` init=0
6. reads handle graph via handles_to()
7. enumerates processes via list_processes()
8. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `co_hits` init=0
- local `foreign_vm_read` init=0
- reads handle graph via handles_to()
- enumerates processes via list_processes()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `co_hits > 0 || foreign_vm_read > 0`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (open_process() AccessMask::VmRead and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 29_process_cooccurrence
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0
```

Open the pair sources beside this lesson:

- `process_cooccurrence/red_example.cpp` — full red multi-step
- `process_cooccurrence/blue_example.cpp` — full blue multi-reason
- `process_cooccurrence/pair.cpp` — StrategyEntry wiring + narrator
