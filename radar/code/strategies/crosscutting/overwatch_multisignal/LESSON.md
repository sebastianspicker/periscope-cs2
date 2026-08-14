# 98_overwatch_multisignal — Overwatch multi-signal score

Family: Detection. Tiers: all. Area: xc/ops. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Handle + vpn + unique build; overwatch_score starts 0

Blue: Score≥2 from multi signals; queue; multi_reason handle+vpn+poly

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::overwatch_multisignal::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Weak multi-scars (handle+vpn+poly) — Mild handle + vpn + unique build; overwatch_score starts 0.
2. Step 1: mild scar — external VmRead handle (weak signal #1).
3. Step 2: weak signal #2 — VPN / proxy path.
4. Step 3: weak signal #3 — polymorphic per-buyer build id (not shared).
5. Step 4: red hopes score stays low / delayed — leave score at 0.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- vpn_proxy_active — World.vpn_proxy_active = true
- binary_build_id — World.binary_build_id = "buyer_msig_9f"
- overwatch_score — World.overwatch_score = 0
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.

Achieved when: `r.mild_handle && r.vpn && r.unique_build && r.score_initial < 1.0`

## BLUE

Entry: `examples::overwatch_multisignal::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Multi-signal overwatch score — Score handle+vpn+poly; queue when score>=2; multi_reason.
2. Step 1: collect weak signals (handle / vpn / poly).
3. Step 2: multi-invariant delayed scorer + FP budget.
4. Step 3: seller fusion if multi-family scars present.
5. Step 4: aggregate score — handle + vpn + poly each contribute 1.0.
6. Step 5: queue when score ≥ 2 from multi signals.

Team / depth sensors:
- `depth::MultiInvariantScorer`
  - RichDemoFrame path: vision_hits, latency, peer reports, delayed action / overwatch threshold.
- `depth::RichDemoFrame`
- `depth::apply_overwatch_to_world`
- `depth::SellerFusionCorrelator`

Multi-reason / result fields and sensors:
- result field `handle_sig`
- result field `vpn_sig`
- result field `poly_sig`
- result field `score`
- result field `queued`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- local `sig_count` init=(r.handle_sig ? 1 : 0) + (r.vpn_sig ? 1 : 0) + (r.poly_sig ? 1 : 0)
- reads handle graph via handles_to()
- filters AccessMask::VmRead handles

Win conditions for this pair:
- detected := `r.score >= 2.0`
- mitigated := `r.queued`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (vpn_proxy_active and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 98_overwatch_multisignal
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `overwatch_multisignal/red_example.cpp` — full red multi-step
- `overwatch_multisignal/blue_example.cpp` — full blue multi-reason
- `overwatch_multisignal/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/multi_invariant_scorer.hpp`
- `depth/seller_fusion.hpp`
