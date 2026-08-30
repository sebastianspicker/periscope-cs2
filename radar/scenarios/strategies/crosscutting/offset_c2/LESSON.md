# 14_offset_c2 — Offset / schema C2

Family: Evasion. Tiers: all. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Post-patch encrypted offset delivery

Blue: Detect readers; C2 is supportive intel

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::offset_c2::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Offset auto-update C2 (multi-step) — Reader + VM_READ; pull encrypted schema from offsets CDN; optional 
2. Step 1: external reader process.
3. Step 2: open VM_READ on game (primary scar).
4. Step 3: post-patch encrypted offset / schema C2.
5. Step 4: optional second net — radar SaaS delivery (supportive).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- Process.reader_active — Process.reader_active = true
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- spawn() — Spawn actor process on World process list.
- add_net() — Plant NetFlow residual (radar SaaS / C2).

Achieved when: `r.handle_open && r.c2_net`

## BLUE

Entry: `examples::offset_c2::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Cannot ban 'offsets' — Schema churn is red ops excellence.
2. Narrator counter: Handle ∧ C2 correlation — C2 supportive; strong detect requires handle + offset intel.
3. reads handle graph via handles_to()
4. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `handle_hit`
- result field `c2_intel`
- result field `multi_reason`
- result field `detected`
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.handle_hit && r.c2_intel`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (Process.reader_active and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 14_offset_c2
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `offset_c2/red_example.cpp` — full red multi-step
- `offset_c2/blue_example.cpp` — full blue multi-reason
- `offset_c2/pair.cpp` — StrategyEntry wiring + narrator
