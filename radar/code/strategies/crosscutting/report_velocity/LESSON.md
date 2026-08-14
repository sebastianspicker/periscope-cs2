# 51_report_velocity — Report velocity

Family: Detection. Tiers: all. Area: xc/ops. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: ≥3 accounts same payment high reports + clean control

Blue: multi_reason high_report_accounts&&payment_cluster

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::report_velocity::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Report-velocity farm — ≥3 smurfs same payment/IP with high reports; one clean control.
2. Step 1: farm seats — same payment + IP class, elevated report counts.
3. Step 2: clean control account (noise / cover) — different payment.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- add_account() — Plant Account row (hwid/payment/ip/report_count graph).

Achieved when: `r.farm_accounts >= 3 && r.report_sum >= 20`

## BLUE

Entry: `examples::report_velocity::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Report velocity multi-reason — high_report_accounts AND payment_cluster_sum — not one account.
2. multi_reason: need BOTH high-report accounts AND payment-cluster sum —
3. local `kReportThreshold` init=10
4. local `kPaymentClusterSumThreshold` init=25

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `linked_payment_report_sum`
- result field `payment_cluster`
- result field `multi_reason`
- result field `detected`
- result field `high_report_accounts`
- local `kReportThreshold` init=10
- local `kPaymentClusterSumThreshold` init=25

Win conditions for this pair:
- detected := `r.multi_reason`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (add_account() and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 51_report_velocity
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `report_velocity/red_example.cpp` — full red multi-step
- `report_velocity/blue_example.cpp` — full blue multi-reason
- `report_velocity/pair.cpp` — StrategyEntry wiring + narrator
