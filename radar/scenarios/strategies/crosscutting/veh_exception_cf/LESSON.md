# 63_veh_exception_cf — VEH exception CF

Family: Evasion. Tiers: all. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Elevate VEH handlers or dirty veh_chain_clean

Blue: Detect !veh_chain_clean || veh_handlers > baseline

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::veh_exception_cf::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: VEH exception control-flow (multi-step) — Handlers ≥4, dirty chain; plant thread-hide / helper handle correlate.
2. Step 1: install extra VEH handlers for control-flow / anti-debug paths.
3. Step 2: break chain integrity (unregistered / tampered VEH list).
4. Step 3: delivery correlate — thread hide (or analysis_host) optional scar.
5. Step 4: optional helper with open handle (delivery surface correlate).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- veh_handlers — World.veh_handlers = 5
- veh_chain_clean — World.veh_chain_clean = false
- thread_hide_from_debugger — World.thread_hide_from_debugger = true
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- spawn() — Spawn actor process on World process list.

Achieved when: `r.chain_dirty && r.handlers >= 4`

## BLUE

Entry: `examples::veh_exception_cf::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: VEH chain multi-reason — chain_dirty / handlers_high + debug_correlate (or both VEH flags).
2. Multi-reason: both VEH flags, or one VEH flag + debug correlate.
3. local `kBaselineHandlers` init=1
4. local `veh_flags` init=(r.chain_dirty ? 1 : 0) + (r.handlers_high ? 1 : 0)

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `handlers`
- result field `chain_dirty`
- result field `handlers_high`
- result field `debug_correlate`
- result field `multi_reason`
- result field `detected`
- local `kBaselineHandlers` init=1
- local `veh_flags` init=(r.chain_dirty ? 1 : 0) + (r.handlers_high ? 1 : 0)

Win conditions for this pair:
- detected := `(r.chain_dirty || r.handlers_high) && r.multi_reason`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (veh_handlers and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 63_veh_exception_cf
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `veh_exception_cf/red_example.cpp` — full red multi-step
- `veh_exception_cf/blue_example.cpp` — full blue multi-reason
- `veh_exception_cf/pair.cpp` — StrategyEntry wiring + narrator
