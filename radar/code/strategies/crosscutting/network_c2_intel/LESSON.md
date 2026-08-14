# 28_network_c2_intel — Network / C2 intel

Family: Detection. Tiers: all. Area: xc/ops. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Loader + VmRead handle + auth/offset-C2/radar SaaS

Blue: multi_reason intel_hits&&handle_hit pair_with_client_scar

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::network_c2_intel::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Seller C2 + technical scar — Loader, VmRead handle, auth/offset-C2/radar SaaS nets.
2. Step 1: loader / helper process (external reader surface).
3. Step 2: technical scar — open VM_READ handle to the game.
4. Step 3: multi net — auth + offset C2 + radar SaaS (intel surface).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.
- add_net() — Plant NetFlow residual (radar SaaS / C2).

Achieved when: `r.handle_open && (r.offset_c2 || r.radar_saas)`

## BLUE

Entry: `examples::network_c2_intel::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: C2 intel + handle (multi-reason) — intel_hits && handle_hit — never domain intel alone.
2. reads handle graph via handles_to()
3. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `handle_hit`
- result field `multi_reason`
- result field `detected`
- result field `intel_hits`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.multi_reason`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (open_process() AccessMask::VmRead and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 28_network_c2_intel
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `network_c2_intel/red_example.cpp` — full red multi-step
- `network_c2_intel/blue_example.cpp` — full blue multi-reason
- `network_c2_intel/pair.cpp` — StrategyEntry wiring + narrator
