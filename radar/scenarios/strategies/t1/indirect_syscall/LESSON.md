# 02_indirect_syscall — Indirect syscalls

Family: Delivery. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Syscall open/read bypasses usermode hooks

Blue: Do not trust ntdll hooks; use handle graph

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::indirect_syscall::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Indirect / direct syscalls — Skip hooked ntdll stubs; still NtOpenProcess/NtReadVirtualMemory.
2. Full shipped T1 client: stage + syscall-path attach + entity pull.

Team / depth APIs used:
- `t1_red::SyscallCheat`
- call `client.run_full_loop()`
- call `client.pid()`

Expanded team path (what the wrapper actually does on sim::World):
- Multi-step T1 path: spawn → attach via syscall-path backend (via_syscall_path handle scar) → pull entities → optional staging/loader steps.
- Indirect syscalls and soft-kernel surfaces still leave process/handle/module scars blue can sample.

World scars and lab surfaces (from shipped red code):
- Handle.via_syscall_path on game target.
- Staging / loader / module scars depending on strategy path.

Achieved when: `rep.attached && rep.via_syscall && rep.entity_count > 0`

## BLUE

Entry: `examples::indirect_syscall::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Naive usermode hooks — Only see non-syscall path → silent.
2. Narrator counter: Handle truth — Object callbacks / handle table ignore path.
3. [t1_blue::T1Agent] full_scan (or path-specific scans): syscall-path handles, staging/loader, parent lineage, module stomp/hollow, ETW-style blinds.
4. [t1_blue::T1Agent] Multi-reason: handle + staging + integrity, not one flag echo.

Team / depth sensors:
- `t1_blue::T1Agent`
  - full_scan (or path-specific scans): syscall-path handles, staging/loader, parent lineage, module stomp/hollow, ETW-style blinds.
  - Multi-reason: handle + staging + integrity, not one flag echo.
- `ac::MemoryTelemetrySink`
- call `agent.full_scan()`

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`

Win conditions for this pair:
- detected := `d.handle_truth_hit && d.syscall_handles > 0`
- mitigated := `d.hooks_blind && d.handle_truth_hit`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Read red_example and blue_example as a duel: red plants multi-step World scars; blue answers with multi-reason detect and/or mitigate. strategy_lab pass encodes that contract.

## Run

```bash
./build/strategy_lab run 02_indirect_syscall
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `indirect_syscall/red_example.cpp` — full red multi-step
- `indirect_syscall/blue_example.cpp` — full blue multi-reason
- `indirect_syscall/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `t1/red/syscall_cheat.hpp`
- `t1/blue/syscall_aware_monitor.hpp`
