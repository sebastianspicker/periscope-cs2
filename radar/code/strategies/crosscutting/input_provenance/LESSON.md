# 22_input_provenance — Input provenance (blue)

Family: Detection. Tiers: all. Area: xc/ops. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Multi-sample injected+raw_hid mix; raw_sendinput_mixed residual

Blue: Correlate inject with raw/mixed/multi_sample (not single echo)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::input_provenance::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Multi-sample inject+raw mix — ≥4 injected + ≥2 raw_hid + serial; raw_sendinput_mixed; no handle.
2. Step 1: NO game handle required — pure ops / input-path sensor abuse.
3. Step 2: multi-sample ≥4 injected events.
4. Step 3: ≥2 raw_hid mix (looks partially legit).
5. Step 4: optional serial_arduino hardware-emulator events.
6. Step 5: residual mixed raw/SendInput path.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- raw_sendinput_mixed — World.raw_sendinput_mixed = true
- push_input() — Push InputEvent (injected vs raw_hid provenance).

Achieved when: `r.injected_count > 0 && r.raw_count > 0`

## BLUE

Entry: `examples::input_provenance::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Input provenance correlation — Count classes; multi_sample; require inject+(raw|mixed|multi).
2. Step 1: count provenance classes across the sample window.
3. Step 2: mixed residual (raw path + inject API improperly blended).
4. Step 3: multi_sample — ≥3 non-raw OR injected>=2 (not a single echo).
5. Step 4: multi_reason correlation — not single injected echo alone.
6. detected = injected>0 && (raw>0 || mixed || multi_sample)

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `mixed`
- result field `multi_sample`
- result field `multi_reason`
- result field `detected`
- result field `injected`
- result field `raw`
- result field `serial`
- result field `kmbox`
- local `non_raw` init=r.injected + r.serial + r.kmbox

Win conditions for this pair:
- detected := `r.multi_reason`
- mitigated := `blue.detected`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (raw_sendinput_mixed and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 22_input_provenance
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `input_provenance/red_example.cpp` — full red multi-step
- `input_provenance/blue_example.cpp` — full blue multi-reason
- `input_provenance/pair.cpp` — StrategyEntry wiring + narrator
