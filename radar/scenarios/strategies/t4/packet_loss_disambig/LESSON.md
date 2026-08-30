# 96_packet_loss_disambig — Packet loss vs lag-switch disambig

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: packet_loss_faked without lag_switch; net residual optional

Blue: Disambig loss_only; multi-reason + fog

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::packet_loss_disambig::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Fake packet loss — Simulate sequence gaps without lag_switch hold; optional net scar.
2. Step 1: simulated loss / sequence gaps — deliberately not lag_switch.
3. Step 2: optional net residual (lossy proxy / drop injector).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- packet_loss_faked — World.packet_loss_faked = true
- lag_switch_active — World.lag_switch_active = false
- spawn() — Spawn actor process on World process list.
- add_net() — Plant NetFlow residual (radar SaaS / C2).

Achieved when: `r.packet_loss && !r.lag_switch`

## BLUE

Entry: `examples::packet_loss_disambig::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Loss vs lag-switch disambig — Detect packet_loss_faked; classify loss_only vs both.
2. Narrator counter: Interest management residual — Fog starves free enemy origin under desync/loss residual.
3. Multi-reason: loss residual + disambig class (loss_only vs both) + net.
4. structural fog / stream surfaces

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `packet_loss`
- result field `lag_switch`
- result field `both`
- result field `loss_only`
- result field `net_hit`
- result field `detected`
- result field `mitigated`
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `r.packet_loss && (r.loss_only || r.both || r.net_hit)`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (packet_loss_faked and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 96_packet_loss_disambig
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `packet_loss_disambig/red_example.cpp` — full red multi-step
- `packet_loss_disambig/blue_example.cpp` — full blue multi-reason
- `packet_loss_disambig/pair.cpp` — StrategyEntry wiring + narrator
