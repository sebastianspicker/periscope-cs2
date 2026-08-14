# Cross-cutting strategies

Apply on top of **any** delivery tier. Not a fourth parallel “how to read memory.”

| Folder | Meaning |
|--------|---------|
| `evasion/` | Hide the reader (staging, crypto, offsets C2, HWID) |
| `features/` | Product UX (overlay, phone, humanization, input synth) |
| `structural/` | Server/design wins (fog, info-advantage, delayed ban, fallbacks) |
| `ops/` | Graphs & intel (accounts, C2, input provenance) |

```bash
./build/strategy_lab run 12_staged_loader
./build/strategy_lab run 24_interest_mgmt
./build/strategy_lab run 25_info_advantage
```

Full list: `tools/strategy_lab/CATALOG.md`.
