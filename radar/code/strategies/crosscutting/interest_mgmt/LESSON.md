# 24_interest_mgmt — Server interest management

Family: Structural. Tiers: all. Area: xc/structural. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Read whatever client has

Blue: Don't send free enemy XY when not observable

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::interest_mgmt::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Any memory radar — Wants full enemy origin replication; attempts stream key exfil.
2. Step 1+2: want full replication + attempt stream key exfil if encrypted.

Team / depth APIs used:
- `depth::run_stream_exfil_red`

Expanded team path (what the wrapper actually does on sim::World):
- [depth::run_stream_exfil_red] Want full replication when server_sends_full_enemy_origin; attempt stream key exfil if encrypted.
- [depth::run_stream_exfil_red] useful_radar when entity stream still feeds a radar without client inject.

World scars and lab surfaces (from shipped red code):
- server_sends_full_enemy_origin / entity stream crypto / stream_key_exfiltrated surfaces.
- useful_radar when replication still feeds radar without inject.

Achieved when: `r.full_replication_wanted && r.useful_radar`

## BLUE

Entry: `examples::interest_mgmt::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Interest management / fog-of-war — Strict fog + encrypt; show leakage_before vs leakage_after.
2. Step 1: score red-friendly partial leak (pedagogy: show leakage risk).
3. Step 2: multi-step mitigate — strict fog + encrypt + strip key.

Team / depth sensors:
- `depth::LeakageScorer`
- `server::Observer`
- `depth::EntityTruth`
- `depth::FogPolicy`
- `depth::StreamCryptoState`

Multi-reason / result fields and sensors:
- result field `leakage_before`
- result field `leakage_after`
- result field `fog_applied`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- local `leakage_cut` init=r.leakage_after < r.leakage_before ||
                           r.leakage_after < 0.15
- local `reasons` init=0
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `false`
- mitigated := `after.structural_kill && !w.server_sends_full_enemy_origin && r.fog_applied`
- Pass narrative: mitigation-only (fog/policy/structural) — detect may stay false; that is still a blue win.

## Takeaway

Blue can constrain or fog the advantage without a classic client scar detect. Check mitigated and server-side flags; structural lessons still count as blue wins.

## Run

```bash
./build/strategy_lab run 24_interest_mgmt
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `interest_mgmt/red_example.cpp` — full red multi-step
- `interest_mgmt/blue_example.cpp` — full blue multi-reason
- `interest_mgmt/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/leakage_scorer.hpp`
