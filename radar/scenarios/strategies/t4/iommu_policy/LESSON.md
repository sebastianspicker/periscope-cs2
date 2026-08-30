# 61_iommu_policy — IOMMU ranked policy

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: DMA+IOMMU-off + optional dma_read; policy unset by red

Blue: ranked_requires_iommu; multi-reason deny + fog

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::iommu_policy::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: DMA without IOMMU — PCIe DMA present; remapping off; optional dma_read; policy unset.
2. Step 1: DMA residual on a host where remapping is off.
3. Step 2: optional off-box dma_read success (entities from game pages).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.dma_device_present — HostTrust.dma_device_present = true
- trust.iommu_on — HostTrust.iommu_on = false
- trust.ranked_requires_iommu — HostTrust.ranked_requires_iommu = false

Achieved when: `r.dma_present && r.iommu_off && r.policy_unset && r.process_list_clean`

## BLUE

Entry: `examples::iommu_policy::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Ranked IOMMU policy — Set ranked_requires_iommu; deny non-compliant hosts.
2. Narrator counter: DMA + IOMMU-off multi-reason — Detect dma_device_present && !iommu_on; fog + policy deny.
3. Step 1: blue policy path — ranked requires IOMMU.
4. Step 2: multi-reason platform signal.
5. Step 3: structural residual — fog when non-compliant host.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `policy_on`
- result field `iommu_off_dma`
- result field `policy_deny`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- local `reasons` init=0
- reads HostTrust platform fields
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `r.iommu_off_dma || r.policy_deny`
- mitigated := `!w.server_sends_full_enemy_origin && (r.policy_deny || r.policy_on)`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (trust.dma_device_present and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 61_iommu_policy
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `iommu_policy/red_example.cpp` — full red multi-step
- `iommu_policy/blue_example.cpp` — full blue multi-reason
- `iommu_policy/pair.cpp` — StrategyEntry wiring + narrator
