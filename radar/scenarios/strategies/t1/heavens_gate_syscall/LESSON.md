# 58_heavens_gate_syscall — Heaven's Gate syscall

Family: Delivery. Tiers: T1. Area: t1. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: 32-bit WOW64 stub opens a raw 64-bit handle via Heaven's Gate

Blue: Detect WOW64 transition + foreign VM_READ handle anomaly

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::heavens_gate_syscall::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Heaven's Gate — a 32-bit WOW64 process jumps to 64-bit mode to bypass 32-bit ntdll wrappers and open a raw 64-bit VM_READ handle.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() a `wow64-stub.exe` actor process.
- Set World.heavens_gate_transition and World.heavens_gate_stub_pid to model the 32→64-bit transition.
- open_process() from the stub to the game with VmRead (not via_syscall_path, not via_proxy).
- read_mem() a 4-byte entity sample through that raw handle.
- Verify the gate handle in the handle graph (raw, non-proxy, non-syscall-path).

World scars and lab surfaces (from shipped red code):
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- heavens_gate_transition — World.heavens_gate_transition = true.
- heavens_gate_stub_pid — World.heavens_gate_stub_pid = stub pid.
- spawn() — Spawn actor process on World process list.

Achieved when: `gate_handle && heavens_gate_transition && heavens_gate_stub_pid == stub`

## BLUE

Entry: `examples::heavens_gate_syscall::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Do not trust ntdll hook visibility — read the handle graph instead.
2. Direct World reads + local scoring in blue_example.cpp.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles
- checks Handle.via_syscall_path / Handle.via_proxy markers
- inspects Process.name for the WOW64 stub

Win conditions for this pair:
- detected := `foreign_vm_read_handle && (heavens_gate_transition || wow64_process_with_handle) && signals >= 2`
- mitigated := `risk >= 0.6` (risk = signals * 0.20 capped at 1.0)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Heaven's Gate is a classic 32-bit cheat trick for reaching 64-bit syscalls without a clean ntdll path. The transition itself and the resulting raw handle are both observable, so blue wins by reasoning over the handle graph and the process set rather than usermode hook coverage.

## Run

```bash
./build/strategy_lab run 58_heavens_gate_syscall
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `heavens_gate_syscall/red_example.cpp` — full red multi-step
- `heavens_gate_syscall/blue_example.cpp` — full blue multi-reason
- `heavens_gate_syscall/pair.cpp` — StrategyEntry wiring + narrator
