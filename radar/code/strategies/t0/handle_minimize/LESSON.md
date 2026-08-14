# 17_handle_minimize — Handle access minimization

Family: Evasion. Tiers: T0-T1. Area: t0. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Narrow rights / brief reopen

Blue: Continuous handle sampling still sees edges

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::handle_minimize::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Handle minimization — Request only VM_READ; reopen briefly each tick; avoid ALL_ACCESS.

Team / depth APIs used:

World scars and lab surfaces (from shipped red code):
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- read_mem() — ReadProcessMemory-style entity/bytes pull through handle.
- spawn() — Spawn actor process on World process list.
- brief_reopen — brief_reopen=true scar

Achieved when: `count > 0`

## BLUE

Entry: `examples::handle_minimize::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Still a handle — Least privilege does not remove the edge from the graph.
2. local `foreign` init=0
3. local `brief` init=0
4. reads handle graph via handles_to()
5. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- local `foreign` init=0
- local `brief` init=0
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `foreign > 0`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Hide-on-enum races beat a single handle sample. Blue needs multi-sample composition (visible vs continuous/hidden) plus lineage — not one EnumHandles call.

## Run

```bash
./build/strategy_lab run 17_handle_minimize
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T0-T1
```

Open the pair sources beside this lesson:

- `handle_minimize/red_example.cpp` — full red multi-step
- `handle_minimize/blue_example.cpp` — full blue multi-reason
- `handle_minimize/pair.cpp` — StrategyEntry wiring + narrator
