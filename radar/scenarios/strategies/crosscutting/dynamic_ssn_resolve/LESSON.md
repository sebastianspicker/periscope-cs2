# 66_dynamic_ssn_resolve — Dynamic SSN resolve

Family: Evasion. Tiers: T1. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Resolve syscall numbers from ntdll on disk, defeats SSN fingerprinting

Blue: Detect via handle graph + syscall path (SSN fingerprint still possible)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::dynamic_ssn_resolve::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Dynamic SSN resolution — parse ntdll export stubs at runtime so hardcoded syscall tables break on new Windows builds.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() an `ssn-resolver.exe` actor process.
- Set World.dynamic_ssn_resolved and World.resolved_ssn_windows_build = 22631 (models export/stub parsing from ntdll on disk).
- open_process() from the resolver to the game with VmRead over the syscall path.
- read_mem() a 4-byte entity sample; verify the syscall-path handle in the handle graph.

World scars and lab surfaces (from shipped red code):
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- dynamic_ssn_resolved — World.dynamic_ssn_resolved = true.
- resolved_ssn_windows_build — World.resolved_ssn_windows_build = 22631.
- spawn() — Spawn actor process on World process list.

Achieved when: `dynamic_ssn_resolved && syscall_handle`

## BLUE

Entry: `examples::dynamic_ssn_resolve::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: ntdll stub parsing is itself a reason — why is a process reading exports for syscall numbers?
2. Narrator counter: The syscall-path handle stays visible — dynamic SSNs change numbers, not the handle scar.
3. Narrator counter: SSN-to-operation fingerprinting remains viable even with build-correct numbers.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles
- checks Handle.via_syscall_path
- checks World.dynamic_ssn_resolved / resolved_ssn_windows_build

Win conditions for this pair:
- detected := `dynamic_ssn_resolved || (foreign_vm_read_handle && syscall_vm_read_handle)`
- mitigated := `risk >= 0.6` (risk = signals * 0.20 capped at 1.0)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Runtime SSN resolution is a maintenance fix, not an evasion fix: it removes hardcoded tables but leaves both the parsing activity and the syscall-path handle observable. Blue still wins on the handle graph and on per-operation SSN correlation, which is how real syscall monitors fingerprint behavior regardless of the numbers used.

## Run

```bash
./build/strategy_lab run 66_dynamic_ssn_resolve
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `dynamic_ssn_resolve/red_example.cpp` — full red multi-step
- `dynamic_ssn_resolve/blue_example.cpp` — full blue multi-reason
- `dynamic_ssn_resolve/pair.cpp` — StrategyEntry wiring + narrator
