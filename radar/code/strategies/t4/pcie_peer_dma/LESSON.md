# 72_pcie_peer_dma - PCIe peer-to-peer DMA

Family: Delivery. Tiers: T4. Area: t4. Sim only - no real malware, DMA, PCIe configuration, ATS, or ACS manipulation.

## Battlefield

Red: Two PCIe devices transfer entity data directly between endpoints while the IOMMU remains enabled.

Blue: Correlate peer DMA activity, transactions, a clean local process list, and an IOMMU bypass signal. Mitigation requires platform anti-cheat support with correct IOMMU grouping plus ACS/ATS enforcement.

Arena: `sim::World` only. The pair uses the existing peer-DMA scars and does not perform hardware access.

## RED

Entry: `examples::pcie_peer_dma::apply` in `red_example.cpp` / `red_example.hpp`.

1. Enables the simulated DMA-capable hardware while retaining `trust.iommu_on`.
2. Models Device A reading the seeded entity page and Device B receiving it through the dedicated peer-DMA scars.
3. Records four peer transactions and a successful IOMMU bypass with no local process or handle.

Achieved when: `pcie_peer_dma_active && pcie_peer_bypassed_iommu && entities_obtained`.

## BLUE

Entry: `examples::pcie_peer_dma::detect` in `blue_example.cpp` / `blue_example.hpp`.

Blue scores active peer DMA, transaction count, the bypass confirmation, DMA hardware, clean process inventory, and the still-enabled IOMMU. Detection succeeds on active peer DMA or the full corroborated bypass posture. The `0.90` mitigation threshold represents the unusually difficult platform controls needed to constrain P2P TLPs.

## Run

```bash
./build/strategy_lab run 72_pcie_peer_dma
```
