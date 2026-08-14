# 84_lag_switch — Lag switch residual

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: lag_switch_active + net residual; distinct from packet_loss

Blue: Detect lag; disambig vs packet_loss; fog mitigate

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::lag_switch::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Lag switch + net residual — Hold/drop packets for positional advantage; optional C2 net flow.
2. Step 1: artificial latency / packet hold — primary residual.
3. Step 2: optional net residual (relay / desync tool phone-home).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- lag_switch_active — World.lag_switch_active = true
- packet_loss_faked — World.packet_loss_faked = false
- spawn() — Spawn actor process on World process list.
- add_net() — Plant NetFlow residual (radar SaaS / C2).

Achieved when: `r.lag_switch && !r.packet_loss`

## BLUE

Entry: `examples::lag_switch::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Latency / desync sensor — Flag lag_switch_active; disambiguate from packet_loss_faked.
2. Narrator counter: Interest management residual — Fog starves free enemy origin under desync.
3. Multi-reason: lag residual, optionally reinforced by net C2 scar.
4. structural fog / stream surfaces

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `lag_switch`
- result field `packet_loss`
- result field `disambig_lag_only`
- result field `net_hit`
- result field `detected`
- result field `mitigated`
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `r.lag_switch && (r.disambig_lag_only || r.net_hit || r.lag_switch)`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (lag_switch_active and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 84_lag_switch
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `lag_switch/red_example.cpp` — full red multi-step
- `lag_switch/blue_example.cpp` — full blue multi-reason
- `lag_switch/pair.cpp` — StrategyEntry wiring + narrator
