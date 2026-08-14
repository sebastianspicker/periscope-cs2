# 59_enhanced_stack_spoof — Enhanced stack spoof

Family: Evasion. Tiers: T1. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Forge a multi-frame call stack to defeat attribution

Blue: Detect the spoofed stack but struggle with origin attribution

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::enhanced_stack_spoof::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Enhanced stack spoof — forge a four-frame, trusted-module-looking return chain (ntdll → kernel32 → game.dll → reader) before the read.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- spawn() a `legit-reader.exe` actor process.
- open_process() from the reader to the game with VmRead over the syscall path.
- Set World.enhanced_stack_spoof, World.spoofed_call_depth = 4, World.stack_spoof_on_read.
- read_mem() a 4-byte entity sample; verify the handle graph is non-empty.

World scars and lab surfaces (from shipped red code):
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- enhanced_stack_spoof — World.enhanced_stack_spoof = true.
- spoofed_call_depth — World.spoofed_call_depth = 4.
- stack_spoof_on_read — World.stack_spoof_on_read = true.
- spawn() — Spawn actor process on World process list.

Achieved when: `handle_open && spoofed_call_depth >= 2`

## BLUE

Entry: `examples::enhanced_stack_spoof::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Deep forged return chains exceed a shallow normal call chain.
2. Narrator counter: The foreign VM_READ handle survives the spoof — origin attribution degrades, but the scar does not.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles
- checks World.stack_spoof_on_read / enhanced_stack_spoof / spoofed_call_depth

Win conditions for this pair:
- detected := `foreign_vm_read_handle && (enhanced_stack_spoof || stack_spoof_on_read) && signals >= 2`
- mitigated := `risk >= 0.55` (risk = signals * 0.18 capped at 1.0; attribution is less certain)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Stack spoofing attacks attribution, not presence: a forged trusted-module chain makes "who called" ambiguous but does not remove the foreign read handle or the deep-chain anomaly. Real stack-walking monitors therefore shift from origin identification to behavior-level scoring, which is why this pair's mitigation threshold is deliberately lower than its detection one.

## Run

```bash
./build/strategy_lab run 59_enhanced_stack_spoof
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T1
```

Open the pair sources beside this lesson:

- `enhanced_stack_spoof/red_example.cpp` — full red multi-step
- `enhanced_stack_spoof/blue_example.cpp` — full blue multi-reason
- `enhanced_stack_spoof/pair.cpp` — StrategyEntry wiring + narrator
