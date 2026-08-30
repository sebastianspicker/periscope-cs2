# 26_delayed_ban — Delayed / correlated bans

Family: Structural. Tiers: all. Area: xc/structural. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Stay under instant thresholds with weak multi-scars

Blue: Multi-signal delayed confidence bans from world scars

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::delayed_ban::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Plant weak multi-scars under instant ban — Mild foreign VmRead + vpn/private build; overwatch_score stays 0.
2. Step 1: mild foreign VmRead handle (weak scar — under instant ban).
3. Step 2: second weak scar — VPN/proxy path.
4. Step 3: private / unique binary_build_id (poly family, not shared hash).
5. Step 4 (optional): low-report account — not enough alone for instant ban.
6. Step 5: red hopes overwatch/delay — leave score at 0 (under threshold).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- vpn_proxy_active — World.vpn_proxy_active = true
- binary_build_id — World.binary_build_id = "buyer_delayed_a7"
- overwatch_score — World.overwatch_score = 0
- overwatch_queued — World.overwatch_queued = false
- silent_aim_active — World.silent_aim_active = false
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.
- add_account() — Plant Account row (hwid/payment/ip/report_count graph).
- build_watermark — mutate World.build_watermark

Achieved when: `r.mild_handle && r.second_weak`

## BLUE

Entry: `examples::delayed_ban::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: World-scar RiskAggregator — Ingest HandleToGame / vpn / poly from live world scars.
2. Narrator counter: Delayed BanCorrelator — Combine weak signals + multi-invariant; ban after confidence.
3. Step 1: ingest WORLD scars into RiskAggregator (not hard-coded fakes only).
4. Step 2: multi-invariant depth path from world vpn+handle context.
5. Optional: InfoAdvantageHit when multi-invariant crosses.
6. Step 3: BanCorrelator on real aggregated risk + info score.

Team / depth sensors:
- `ac::RiskAggregator`
- `ac::EventKind`
- `depth::MultiInvariantScorer`
  - RichDemoFrame path: vision_hits, latency, peer reports, delayed action / overwatch threshold.
- `depth::RichDemoFrame`
- `server::BanCorrelator`
- `server::BanAction`

Multi-reason / result fields and sensors:
- result field `handle_sig`
- result field `vpn_sig`
- result field `poly_sig`
- result field `info_score`
- result field `risk_score`
- result field `reason`
- result field `detected`
- result field `mitigated`
- result field `multi_reason`
- local `sigs` init=(r.handle_sig ? 1 : 0) + (r.vpn_sig ? 1 : 0) +
                   (r.poly_sig ? 1 : 0)
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `d.action != server::BanAction::None`
- mitigated := `d.action == server::BanAction::DelayedBanCandidate || d.action == server::BanAction::FlagOverwatch || d.action == server::BanAction::SoftRestrict`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (vpn_proxy_active and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 26_delayed_ban
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `delayed_ban/red_example.cpp` — full red multi-step
- `delayed_ban/blue_example.cpp` — full blue multi-reason
- `delayed_ban/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/multi_invariant_scorer.hpp`
- `server/ban_correlator.hpp`
