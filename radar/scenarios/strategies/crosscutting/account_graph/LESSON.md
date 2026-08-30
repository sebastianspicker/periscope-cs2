# 23_account_graph — Account / social graph

Family: Detection. Tiers: all. Area: xc/ops. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Multi-account payment/IP links + VPN + build/C2

Blue: SellerFusion multi_reason linked_pairs&&(vpn||cluster)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::account_graph::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Smurf / account rotation — Multi-account same payment/IP different HWID; VPN + build + C2.
2. Step 1–N: multi-family seller cluster residual.

Team / depth APIs used:
- `depth::SellerFusionCorrelator`

World scars and lab surfaces (from shipped red code):
- add_account() — Plant Account row (hwid/payment/ip/report_count graph).

Achieved when: `r.accounts_added >= 2 && r.vpn && r.multi_family`

## BLUE

Entry: `examples::account_graph::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: SellerFusion graph — linked_pairs + (vpn || multi-family cluster) — not single flag.
2. multi_reason: linked graph AND (vpn OR multi-family cluster) —

Team / depth sensors:
- `depth::SellerFusionCorrelator`
- `depth::ClusterAction`

Multi-reason / result fields and sensors:
- result field `linked_pairs`
- result field `vpn`
- result field `cluster`
- result field `multi_reason`
- result field `detected`

Win conditions for this pair:
- detected := `r.multi_reason`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (add_account() and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 23_account_graph
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `account_graph/red_example.cpp` — full red multi-step
- `account_graph/blue_example.cpp` — full blue multi-reason
- `account_graph/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/seller_fusion.hpp`
