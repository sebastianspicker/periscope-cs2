# Code — tier-first lab

```text
code/
├── shared/          common · sim · lab · server
├── tiers/           t0…t4  (red, blue, strategies, demos)
├── crosscutting/    evasion · features · structural · ops
└── tools/strategy_lab/
```

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Entry points

| Binary | Purpose |
|--------|---------|
| `strategy_lab` | All 28 red/blue strategy pairs |
| `duel_t0`…`duel_t4` | Narrated tier walkthroughs |
| `proto_tN_red/blue` | Focused surface demos |

```bash
./build/strategy_lab list
./build/strategy_lab run 01_external_rpm
./build/duel_t0
./build/duel_t4
```

## Per-tier map

| Tier | Path |
|------|------|
| T0 | `tiers/t0_usermode_rpm/` |
| T1 | `tiers/t1_syscall_soft/` |
| T2 | `tiers/t2_kernel_byovd/` |
| T3 | `tiers/t3_hypervisor/` |
| T4 | `tiers/t4_dma_residual/` |

Open each tier’s `README.md` + `LESSON.md`.
