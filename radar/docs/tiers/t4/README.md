# T4 — DMA / hardware residual

Scar: often no local cheat process.  
Blue: IOMMU/device policy (limited) + interest management + info-advantage.

## Layout

| Path | Role |
|------|------|
| `red/` | `t4_red::DmaRadar` — off-box DMA path (sim) |
| `blue/` | `t4_blue::DmaDefense` — platform signal + fog |
| `strategies/dma_hardware/` | Catalog pair `06_dma_hardware` + thin wrappers |
| `demos/duel/` | Narrated T4 walkthrough |

## Run

```bash
./build/duel_t4
./build/strategy_lab run 06_dma_hardware
```

## Strategies

| Dir | Topic |
|-----|--------|
| `dma_hardware` | PCIe DMA / 2nd PC model |

See [LESSON.md](LESSON.md). Sim only — no real DMA.
