# 20_fallback_chain — Tier fallback chain

Family: Delivery. Tiers: all. Area: xc/structural. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Degrade HV→kernel→RPM automatically

Blue: Always run full detector stack

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::fallback_chain::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Fallback chain T3→T2→T0 — try_start_personal_hv denied under VBS; load driver; open handle.
2. Step 1 (T3): personal HV — denied when VBS/HVCI is on (default arena).
3. Step 2 (T2): custom mem-rw driver fallback after HV deny.
4. Step 3 (T0): usermode RPM handle — always available last resort.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- load_driver() — Load Driver into World.drivers (kernel image scar).
- create_device() — Create Device node linked to driver.
- try_start_personal_hv() — Personal HV attempt (may fail under VBS/HVCI).
- open_process() — OpenProcess-style handle scar (owner→target, access mask).
- spawn() — Spawn actor process on World process list.

Achieved when: `r.hv_failed && (r.handle_open || r.driver_loaded)`

## BLUE

Entry: `examples::fallback_chain::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Run ALL lower detectors always — Hunting only T3 misses the RPM fallback — stack handle+driver.
2. Sensor A: foreign VmRead handle (T0 RPM fallback scar).
3. Sensor B: mem-rw providing driver (T2 kernel fallback scar).
4. reads handle graph via handles_to()
5. inspects drivers/devices
6. filters AccessMask::VmRead handles

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `handle_hit`
- result field `driver_hit`
- result field `multi_reason`
- result field `detected`
- reads handle graph via handles_to()
- inspects drivers/devices
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.handle_hit && r.driver_hit`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (load_driver() and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 20_fallback_chain
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `fallback_chain/red_example.cpp` — full red multi-step
- `fallback_chain/blue_example.cpp` — full blue multi-reason
- `fallback_chain/pair.cpp` — StrategyEntry wiring + narrator
