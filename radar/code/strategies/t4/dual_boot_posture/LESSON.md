# 50_dual_boot_posture — Dual-boot posture

Family: Delivery. Tiers: T4. Area: t4. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Dual-boot cheat OS + PCR + DMA residual

Blue: Multi-sensor dual_boot/PCR/DMA; fog + ranked deny

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::dual_boot_posture::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Dual-boot cheat OS + DMA — Alternate OS with cheat stack; PCR leaves known_good; DMA residual.
2. Step 1: cheat OS path — alternate dual-boot with non-known-good PCR.
3. Step 2: DMA residual rides the cheat-OS session (IOMMU often off there).
4. Step 3: optional off-box dma_read while process list stays clean.

Team / depth APIs used:
- Direct World field writes and helpers (no team wrapper class in this pair).

World scars and lab surfaces (from shipped red code):
- trust.dual_boot_profile — HostTrust.dual_boot_profile = true
- trust.boot_pcr_profile — HostTrust.boot_pcr_profile = "cheat_os"
- trust.dma_device_present — HostTrust.dma_device_present = true
- trust.iommu_on — HostTrust.iommu_on = false

Achieved when: `r.dual_boot && r.pcr_profile != "known_good" && r.dma_path && r.process_list_clean`

## BLUE

Entry: `examples::dual_boot_posture::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Boot PCR / dual-boot multi-sensor — Flag dual_boot, bad PCR, and DMA path together.
2. Narrator counter: Interest management residual — Fog + ranked deny when host posture is untrusted.
3. local `sensors` init=0
4. reads HostTrust platform fields
5. structural fog / stream surfaces

Team / depth sensors:
- Direct World reads + local scoring in blue_example.cpp.

Multi-reason / result fields and sensors:
- result field `dual_boot`
- result field `bad_pcr`
- result field `dma_path`
- result field `multi_sensor`
- result field `detected`
- result field `mitigated`
- local `sensors` init=0
- reads HostTrust platform fields
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `r.dual_boot || r.bad_pcr || (r.multi_sensor)`
- mitigated := `!w.server_sends_full_enemy_origin`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Multi-step red plants more than one scar (trust.dual_boot_profile and follow-ons). Blue must compose sensors; a single flag is not the lesson.

## Run

```bash
./build/strategy_lab run 50_dual_boot_posture
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier T4
```

Open the pair sources beside this lesson:

- `dual_boot_posture/red_example.cpp` — full red multi-step
- `dual_boot_posture/blue_example.cpp` — full blue multi-reason
- `dual_boot_posture/pair.cpp` — StrategyEntry wiring + narrator
