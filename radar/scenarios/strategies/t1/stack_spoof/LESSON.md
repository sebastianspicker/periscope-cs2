# 44_stack_spoof — Stack spoof on read

Family: Evasion. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Syscall open/read with stack_spoof_on_read to lie to stack walkers

Blue: Foreign VM_READ detects alone; +stack_spoof_on_read = spoofed origin

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::stack_spoof::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Stack spoof on read — Syscall OpenProcess; set stack_spoof_on_read; read entities.

Team / depth APIs used:

World scars and lab surfaces (from shipped red code):
- stack_spoof_on_read — World.stack_spoof_on_read = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- read_mem() — ReadProcessMemory-style entity/bytes pull through handle.
- spawn() — Spawn actor process on World process list.

Achieved when: `count > 0 && syscall_handle && w.stack_spoof_on_read`

## BLUE

Entry: `examples::stack_spoof::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Naive stack walk — Spoofed return addresses attribute the read elsewhere.
2. Narrator counter: Handle + spoof scar — Foreign VM_READ detects alone; spoof flag marks spoofed origin.
3. Detect on handle alone; also elevate when spoof flag is set.
4. Mitigated: handle graph still works even if stack walkers are lied to.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `foreign_vm_read` init=0
- local `syscall_path_handles` init=0
- local `handle_hit` init=foreign_vm_read > 0
- local `spoof_scar` init=handle_hit && w.stack_spoof_on_read
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `handle_hit || spoof_scar`
- mitigated := `handle_hit && (w.stack_spoof_on_read || syscall_path_handles > 0)`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (stack_spoof_on_read and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 44_stack_spoof
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `stack_spoof/red_example.cpp` — full red multi-step
- `stack_spoof/blue_example.cpp` — full blue multi-reason
- `stack_spoof/pair.cpp` — StrategyEntry wiring + narrator
