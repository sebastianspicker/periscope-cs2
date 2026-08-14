# 63_fpga_smart_dma — FPGA smart DMA

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: FPGA scatter-gather DMA reads entities without OS scars

Blue: Detect via IOMMU policy or PCIe traffic analysis (extremely hard)

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::fpga_smart_dma::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: FPGA smart DMA — a DMA-capable PCIe device with no IOMMU containment performs autonomous scatter-gather entity reads; no process or handle scars.
2. Direct World field writes and helpers (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- Set Trust.dma_device_present and clear Trust.iommu_on; set World.fpga_smart_dma_active.
- dma_read() three 16-byte page fragments from the entity-table page (offsets 0/16/32).
- Record World.fpga_scatter_reads and World.fpga_hidden_rescan.

World scars and lab surfaces (from shipped red code):
- fpga_smart_dma_active — World.fpga_smart_dma_active = true.
- fpga_scatter_reads — World.fpga_scatter_reads = 3.
- fpga_hidden_rescan — World.fpga_hidden_rescan = entities obtained.
- dma_read() — Direct physical read helper (no process or handle scars).

Achieved when: `fpga_smart_dma_active && scatter_reads == 3 && entities_obtained`

## BLUE

Entry: `examples::fpga_smart_dma::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Clean process list is not a pass — DMA leaves no local process or game-memory handle.
2. Narrator counter: IOMMU posture is the primary scar — a DMA-capable device without IOMMU containment.

Team / depth sensors:
- Direct World reads + local risk scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `detected`
- result field `mitigated`
- result field `signals`
- result field `risk`
- checks Trust.dma_device_present / iommu_on / ranked_requires_iommu
- checks World.fpga_scatter_reads / fpga_hidden_rescan
- inspects the process list (expects only game + AC)

Win conditions for this pair:
- detected := `fpga_smart_dma_active || (dma_device_present && !iommu_on && process_list_clean)`
- mitigated := `ranked_requires_iommu && iommu_on`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Smart DMA moves the entire cheat surface off the OS, and detection is driven by IOMMU/VT-d policy plus PCIe traffic analysis rather than process telemetry. This is the real-world rationale for DMA remapping: without an IOMMU, a clean process list gives blue almost nothing to work with.

## Run

```bash
./build/strategy_lab run 63_fpga_smart_dma
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `fpga_smart_dma/red_example.cpp` — full red multi-step
- `fpga_smart_dma/blue_example.cpp` — full blue multi-reason
- `fpga_smart_dma/pair.cpp` — StrategyEntry wiring + narrator
