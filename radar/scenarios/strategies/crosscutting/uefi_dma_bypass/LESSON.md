# 151_uefi_dma_bypass — UEFI DMA remapping bypass

Family: Evasion. Tiers: crosscutting. Area: xc/evasion. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Bypass DMA remapping (IOMMU/VT-d) via DMAR table analysis, ACS clearing, and IOMMU register manipulation

Blue: Detect IOMMU bypass via DMAR integrity checks, ACS polling, and PCIe traffic anomaly analysis

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::uefi_dma_bypass::Red::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: UEFI DMA remapping bypass — analyze the DMAR table, clear ACS source validation, then disable IOMMU remapping so PCIe DMA is possible.
2. Direct World trust/note mutations (no team wrapper class in this pair).

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

Expanded team path (what the wrapper actually does on sim::World):
- Set World.pcie_peer_bypassed_iommu (ACS cleared on root port).
- Set Trust.iommu_on = false and World.iommu_bypass_active / iommu_bypass_confirmed.
- Set Trust.dma_device_present and World.pcie_peer_dma_active.

World scars and lab surfaces (from shipped red code):
- pcie_peer_bypassed_iommu — ACS source-validation cleared.
- iommu_bypass_active — World.iommu_bypass_active = true.
- Trust.iommu_on — disabled.
- pcie_peer_dma_active — PCIe DMA now possible.
- w.note() — DMAR table read / remapping-unit scan residuals.

Achieved when: `w.iommu_bypass_active && !w.trust.iommu_on`

## BLUE

Entry: `examples::uefi_dma_bypass::Blue::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: IOMMU state vs expected baseline — disabled while a bypass is active.
2. Narrator counter: ACS clearing on root ports — the bypass precursor.
3. Narrator counter: Unexpected PCIe DMA transfers with IOMMU disabled.

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `iommu_state_changed`
- result field `acs_cleared_detected`
- result field `pcie_anomaly`
- result field `detection_count`
- checks Trust.iommu_on / dma_device_present
- checks World.iommu_bypass_active / pcie_peer_bypassed_iommu / pcie_peer_dma_active

Win conditions for this pair:
- detected := `blue.detection_count > 0`
- mitigated := `mitigation.detection_count > 0` (Blue::mitigate logs the TPM PCR event and requests firmware re-enable of VT-d)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

DMA remapping bypasses are a pre-OS, hardware-class attack: once IOMMU/VT-d is off, PCIe DMA is uncontained and the OS is largely blind. This is why DMAR/ACS state feeds TPM measurements and why remediation must be a firmware-level re-enable rather than a runtime patch.

## Run

```bash
./build/strategy_lab run 151_uefi_dma_bypass
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `uefi_dma_bypass/red_example.cpp` — full red multi-step
- `uefi_dma_bypass/blue_example.cpp` — full blue multi-reason
- `uefi_dma_bypass/pair.cpp` — StrategyEntry wiring + narrator
