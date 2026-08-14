# T4 Cat-and-Mouse Lesson

Battlefield: DMA / off-box residual + structural fog. Sim only.

## RED goal

Use the shipped team library `t4_red::DmaRadar` (and strategy-specific red_example paths) to plant multi-step World scars and achieve an information or control advantage.

## BLUE goal

Use `t4_blue::DmaDefense` and pair blue_example sensors for multi-reason detect and/or mitigate. Pass is blue_detected || blue_mitigated || !red_achieved.

## Strategy pairs in this tier

- `60_aim_challenge` — strategies/aim_challenge/LESSON.md
- `39_capture_cv_hid` — strategies/capture_cv_hid/LESSON.md
- `97_clipcursor` — strategies/clipcursor/LESSON.md
- `72_desktop_duplication` — strategies/desktop_duplication/LESSON.md
- `06_dma_hardware` — strategies/dma_hardware/LESSON.md
- `50_dual_boot_posture` — strategies/dual_boot_posture/LESSON.md
- `85_external_clone_display` — strategies/external_clone_display/LESSON.md
- `61_iommu_policy` — strategies/iommu_policy/LESSON.md
- `84_lag_switch` — strategies/lag_switch/LESSON.md
- `73_network_multibox_aim` — strategies/network_multibox_aim/LESSON.md
- `96_packet_loss_disambig` — strategies/packet_loss_disambig/LESSON.md

## How to read a pair

1. LESSON.md (this folder's strategies/<name>/) — multi-step red scars and blue sensors.
2. red_example.cpp — exact World mutations and team calls.
3. blue_example.cpp — multi-reason detect/mitigate.
4. pair.cpp — StrategyEntry id, family, narrator.

## Run

```bash
./build/strategy_lab run --tier T4
./build/duel_t4   # if built
```

## Takeaway

Hardware residual may leave the game PC process list clean. IOMMU/device policy + structural fog + behavioral multi-signal are the durable answers.
