# 73_network_multibox_aim — Network multibox aim

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Remote aim stream + multi-sample serial/kmbox + net residual

Blue: Multi-reason multibox or net+HID correlation; fog

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::network_multibox_aim::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Network multibox aim — Second box streams aim; local HID applies deltas; radar net residual.
2. Step 1: second box streams aim decisions; local box only applies HID.
3. Step 2: local serial / KMBox path applies remote deltas (no memory scar).
4. Step 3: net residual — aim/radar stream from the multibox host.
5. Optional second flow for correlation depth.
6. Multi-step achieved: multibox flag + HID + net correlation.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- multibox_net_aim — World.multibox_net_aim = true
- spawn() — Spawn actor process on World process list.
- add_net() — Plant NetFlow residual (radar SaaS / C2).
- push_input() — Push InputEvent (injected vs raw_hid provenance).

Achieved when: `r.multibox && r.bad_input && r.net_flow`

## BLUE

Entry: `examples::network_multibox_aim::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Multibox / net+input multi-reason — Flag multibox_net_aim or correlated radar-net with non-raw HID.
2. Narrator counter: Interest management residual — Fog still applies when multibox residual is present.
3. Multi-reason correlation: direct flag, or (net + HID), or multi samples.
4. local `bad_n` init=0
5. local `net_n` init=0
6. local `reasons` init=0
7. structural fog / stream surfaces

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `multibox`
- result field `bad_input`
- result field `net_hit`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- local `bad_n` init=0
- local `net_n` init=0
- local `reasons` init=0
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `r.multibox || (r.net_hit && r.bad_input) || r.multi_reason`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (multibox_net_aim and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 73_network_multibox_aim
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `network_multibox_aim/red_example.cpp` — full red multi-step
- `network_multibox_aim/blue_example.cpp` — full blue multi-reason
- `network_multibox_aim/pair.cpp` — StrategyEntry wiring + narrator
