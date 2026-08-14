# 06_dma_hardware — DMA / hardware off-box

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: PCIe DMA second machine via full residual loop, no local process

Blue: Multi-sensor IOMMU + fog-of-war + info-advantage

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::dma_hardware::run_red` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: DMA / FPGA / 2nd PC — PCIe reads physical RAM; game PC may have no cheat process.
2. Narrator move: Full residual loop — Enable DMA path, pull entities off-box, plant residual scars.
3. Prefer multi-step run_full_loop; map report fields onto RedResult.

Team / depth APIs used:
- `t4_red::DmaRadar`
- call `radar.run_full_loop()`
- call `radar.entities()`

Expanded team path (what the wrapper actually does on sim::World):
- Enable DMA/FPGA residual path: trust.dma_device_present, often process_list_clean (no game-PC cheat process).
- Off-box entity pull; plant residual scars (capture card, dual-boot PCR, IOMMU off, clone display) when residuals=true.
- Achieved when hardware path is live and either offbox_read_ok or iommu_off allows the lab narrative.

World scars and lab surfaces (from shipped red code):
- HostTrust.dma_device_present; process_list_clean residual.
- Optional iommu_off, capture_card_present, dual_boot_profile.

Achieved when: `rep.process_list_clean && rep.hardware_enabled && (rep.offbox_read_ok || rep.iommu_off)`

## BLUE

Entry: `examples::dma_hardware::run_blue` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: IOMMU / device policy (limited) — Where product allows: DMA remapping, unexpected bus masters.
2. Narrator counter: Multi-sensor residual scan — Platform + residual scars + structural fog + behavioral.
3. Narrator counter: Structural + behavioral residual — Fog-of-war + info-advantage still apply without client scars.
4. Prefer multi-sensor full() when available; map into BlueResult.

Team / depth sensors:
- `t4_blue::DmaDefense`
  - full(): platform_signal (dma_device, iommu_off), residual scars, structural fog (server_sends_full_enemy_origin).
  - Mitigation path: fog_applied / structural_kill even when client process list is clean.
- `t4_blue::detect`
- call `def.full()`

Multi-reason / result fields and sensors:
- result field `dma_device`
- result field `iommu_off`
- result field `detected`
- result field `mitigated`
- reads HostTrust platform fields
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `scan.platform_signal || scan.dma_device || !scan.reasons.empty()`
- mitigated := `scan.fog_applied || scan.structural_kill || !w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

T4 residual: process list may be clean. Platform policy (IOMMU/device) plus structural fog and behavioral multi-signal remain the durable counters.

## Run

```bash
./build/strategy_lab run 06_dma_hardware
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `dma_hardware/red_example.cpp` — full red multi-step
- `dma_hardware/blue_example.cpp` — full blue multi-reason
- `dma_hardware/pair.cpp` — StrategyEntry wiring + narrator
