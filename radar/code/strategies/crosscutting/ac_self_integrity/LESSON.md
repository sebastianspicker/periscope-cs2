# 41_ac_self_integrity — AC self-integrity

Family: Detection. Tiers: all. Area: xc/ops. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Multi-step: AC text_hash=tampered + iat/headers residual

Blue: Dirty is_ac modules + hook residual; multi_reason; fail closed

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::ac_self_integrity::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Tamper AC binary (multi-step) — Locate AC, patch module text_hash + plant iat/headers residual.
2. Step 1: locate anti-cheat process.
3. Step 2: ensure modules exist (push if empty).
4. Step 3: tamper ≥1 module text_hash → "tampered".
5. Step 4: optional secondary residual on AC module

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- Module.text_hash — game module field text_hash="tampered"
- Module.iat_hooked — game module field iat_hooked=true
- Module.headers_erased — game module field headers_erased=true
- Process.hollowed — Process.hollowed = false
- text_hash="tampered" — Module.text_hash set to tampered (integrity scar)

Achieved when: `r.dirty_count >= 1`

## BLUE

Entry: `examples::ac_self_integrity::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: AC self-integrity (fail closed) — Hash own modules + hook residual; multi_reason; untrusted host.
2. Detect on any dirty AC module; fail-closed when multi_reason or text dirty.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- result field `dirty_modules`
- result field `text_dirty`
- result field `hook_residual`
- local `text_dirty` init=(m.text_hash != "clean")
- local `hook` init=m.iat_hooked || m.eat_hooked || m.headers_erased
- inspects Process.modules (IAT/EAT/text_hash/present_hooked)
- checks Module.iat_hooked / eat_hooked
- checks Module.text_hash integrity
- checks PEB link / erased headers

Win conditions for this pair:
- detected := `r.dirty_modules > 0`
- mitigated := `r.detected`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (Module.text_hash and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 41_ac_self_integrity
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `ac_self_integrity/red_example.cpp` — full red multi-step
- `ac_self_integrity/blue_example.cpp` — full blue multi-reason
- `ac_self_integrity/pair.cpp` — StrategyEntry wiring + narrator
