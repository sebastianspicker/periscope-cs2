# 45_etw_blind — ETW pipeline blind

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Set etw_enabled=false then open/read so ETW consumers go dark

Blue: Detect !etw_enabled as pipeline broken; handle graph still works

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::etw_blind::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: ETW blind — Disable etw_enabled; syscall OpenProcess; read entities.

Team / depth APIs used:

World scars and lab surfaces (from shipped red code):
- etw_enabled — World.etw_enabled = false
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- read_mem() — ReadProcessMemory-style entity/bytes pull through handle.
- spawn() — Spawn actor process on World process list.

Achieved when: `count > 0 && syscall_handle && !w.etw_enabled`

## BLUE

Entry: `examples::etw_blind::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: ETW-only sensors — With etw_enabled=false, telemetry consumers go silent.
2. Narrator counter: Pipeline integrity + handle graph — !etw_enabled → pipeline broken; handle graph still detects.
3. Detect on pipeline integrity alone, or on handle graph alone.
4. Mitigated in the lab sense: ETW consumers blind, strong sensors still fire.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `foreign_vm_read` init=0
- local `syscall_path_handles` init=0
- local `etw_broken` init=!w.etw_enabled
- local `handle_hit` init=foreign_vm_read > 0
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `etw_broken || handle_hit`
- mitigated := `etw_broken && handle_hit`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (etw_enabled and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 45_etw_blind
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `etw_blind/red_example.cpp` — full red multi-step
- `etw_blind/blue_example.cpp` — full blue multi-reason
- `etw_blind/pair.cpp` — StrategyEntry wiring + narrator
