# 67_ntdll_hook_evade — ntdll hook evade

Family: Evasion. Tiers: T1. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Detect ntdll hooks and evade via direct syscall or clean copy

Blue: Detect via handle graph (hook telemetry is lost)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::ntdll_hook_evade::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: ntdll hook evade — compare in-memory ntdll.text against the clean disk image, then map a clean copy and read via direct syscall.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() an `unhooker.exe` actor process.
- Set World.ntdll_hooks_detected and World.hooks_evaded_count = 3 (models comparing in-memory ntdll.text with the disk image).
- Set World.used_clean_ntdll_copy.
- open_process() from the unhooker to the game with VmRead over the syscall path; read_mem() a 4-byte entity sample.

World scars and lab surfaces (from shipped red code):
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- ntdll_hooks_detected — World.ntdll_hooks_detected = true.
- hooks_evaded_count — World.hooks_evaded_count = 3.
- used_clean_ntdll_copy — World.used_clean_ntdll_copy = true.
- spawn() — Spawn actor process on World process list.

Achieved when: `ntdll_hooks_detected && syscall_handle`

## BLUE

Entry: `examples::ntdll_hook_evade::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Hook telemetry is lost — a clean copy or direct syscall bypasses the API hooks.
2. Narrator counter: The foreign VM_READ handle survives — handle truth outlives the hook bypass.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles
- checks World.ntdll_hooks_detected / used_clean_ntdll_copy / hooks_evaded_count

Win conditions for this pair:
- detected := `(ntdll_hooks_detected || used_clean_ntdll_copy) || foreign_vm_read_handle`
- mitigated := `risk >= 0.55` (risk = signals * 0.20 capped at 1.0)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Unhooking ntdll blinds API-level monitoring, but it cannot erase the process-level handle or the fact that a clean ntdll copy was mapped. The pair is a textbook case of sensor layering: usermode hook telemetry is a convenience, and handle-graph truth is the durable signal.

## Run

```bash
./build/strategy_lab run 67_ntdll_hook_evade
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `ntdll_hook_evade/red_example.cpp` — full red multi-step
- `ntdll_hook_evade/blue_example.cpp` — full blue multi-reason
- `ntdll_hook_evade/pair.cpp` — StrategyEntry wiring + narrator
