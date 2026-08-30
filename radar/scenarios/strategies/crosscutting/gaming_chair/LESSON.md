# 101_gaming_chair — Gaming chair product suite

Family: Feature. Tiers: all. Area: xc/features. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Multi-step product suite — proxy read path + high-volume entity stream + ESP

Blue: Multi-reason detection on feature residuals (high reads, high volume, product suite)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::gaming_chair::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Gaming-chair suite — compose proxy read, schema/band4, ESP glow, and aimbot into one delivery.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- proxy_hijack: set World.handle_proxy_active, add 250 remote read ops and 1.5M read bytes.
- band4: set World.schema_cache_active / schema_remote_update.
- esp: set World.object_glow_product / glow_weapon.
- aimbot: set World.triggerbot_active.

World scars and lab surfaces (from shipped red code):
- handle_proxy_active — World.handle_proxy_active = true.
- remote_read_ops / remote_read_bytes — high-volume read residuals.
- object_glow_product / schema_cache_active — product-surface residuals.
- w.note() — "gaming_chair:" feature summary.

Achieved when: `r.active >= 2 && w.remote_read_ops > 200`

## BLUE

Entry: `examples::gaming_chair::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: High read counts and high byte volume are volume residuals.
2. Narrator counter: The product suite itself — proxy, glow, or schema cache — corroborates.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected_features`
- result field `count`
- result field `risk`
- result field `detected`
- checks World.remote_read_ops / remote_read_bytes
- checks World.handle_proxy_active / object_glow_product / schema_cache_active

Win conditions for this pair:
- detected := `r.count >= 2`
- mitigated := `false`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win (blue also clears handle_proxy_active / object_glow_product on detect).

## Takeaway

A "gaming chair" is a bundled feature suite, and each bundled feature leaves a measurable residual — read volume, proxy activity, or glow state. Blue wins by composing those residuals rather than hunting a single flag, which mirrors how product suites surface as multi-signal anomalies in telemetry rather than as one clean signature.

## Run

```bash
./build/strategy_lab run 101_gaming_chair
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `gaming_chair/red_example.cpp` — full red multi-step
- `gaming_chair/blue_example.cpp` — full blue multi-reason
- `gaming_chair/pair.cpp` — StrategyEntry wiring + narrator
