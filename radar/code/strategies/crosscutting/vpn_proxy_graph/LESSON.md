# 86_vpn_proxy_graph — VPN / proxy account graph

Family: Detection. Tiers: all. Area: xc/ops. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: vpn_proxy_active; ≥3 accounts share vpn_res_1; optional payment

Blue: multi_reason vpn_flag&&ip_cluster

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::vpn_proxy_graph::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: VPN / proxy multi-account — vpn_proxy_active + ≥3 accounts on vpn_res_1; different HWIDs.
2. Step 1: residential VPN / proxy path for multi-account ops.
3. Step 2: ≥3 accounts share vpn_res_1 ip_class; different HWIDs.
4. Optional shared payment on a subset (graph edge without collapsing HWID).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- vpn_proxy_active — World.vpn_proxy_active = true
- add_account() — Plant Account row (hwid/payment/ip/report_count graph).

Achieved when: `r.vpn_proxy && r.clustered_accounts >= 3`

## BLUE

Entry: `examples::vpn_proxy_graph::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: VPN + IP class multi-reason — vpn_flag AND ip_cluster (vpn_* count>=2) — not either alone.
2. multi_reason: VPN session flag AND shared exit-class cluster
3. local `vpnish` init=cls.rfind("vpn_", 0) == 0 || cls.find("proxy") != std::string::npos

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `vpn_flag`
- result field `max_vpn_class_count`
- result field `ip_cluster`
- result field `multi_reason`
- result field `detected`
- local `vpnish` init=cls.rfind("vpn_", 0) == 0 || cls.find("proxy") != std::string::npos

Win conditions for this pair:
- detected := `r.multi_reason`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Multi-step red plants more than one scar (vpn_proxy_active and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 86_vpn_proxy_graph
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `vpn_proxy_graph/red_example.cpp` — full red multi-step
- `vpn_proxy_graph/blue_example.cpp` — full blue multi-reason
- `vpn_proxy_graph/pair.cpp` — StrategyEntry wiring + narrator
