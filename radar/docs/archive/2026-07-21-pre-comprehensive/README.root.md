# Anti-cheat research lab — external “legit” radar

Tier-first learning project: **red** (adversary surfaces) vs **blue** (defenses), all lab-simulated.

## Layout

```text
anti-cheat-legit-radar/
├── README.md                 ← you are here
├── docs/
│   ├── OVERVIEW.md           ← start here (threat model + map)
│   ├── THREAD-SUMMARY.md
│   ├── THREAT-TIERS.md
│   ├── TIERED-COUNTERS.md
│   └── archive/              ← old parallel red/blue doc trees
└── code/
    ├── shared/               ← common, sim, lab, server
    ├── tiers/
    │   ├── t0_usermode_rpm/  ← red/ blue/ strategies/ demos/
    │   ├── t1_syscall_soft/
    │   ├── t2_kernel_byovd/
    │   ├── t3_hypervisor/
    │   └── t4_dma_residual/
    ├── crosscutting/         ← evasion, features, structural, ops
    └── tools/strategy_lab/   ← catalog runner
```

## Learn (order)

1. [`docs/OVERVIEW.md`](docs/OVERVIEW.md)
2. `code/tiers/t0_usermode_rpm/` → LESSON + `duel_t0`
3. t1 → t2 → t3 (same pattern)
4. `code/crosscutting/` (evasion / features / structural)
5. `./build/strategy_lab all`

## Build

```bash
cd code
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure

./build/strategy_lab list
./build/duel_t0
./build/strategy_lab run 01_external_rpm
```

## Safety

Lab only (`sim::World`). No real games, no loadable malware, no real VMX/BYOVD exploits.

- [Residual VMX/BYOVD/DMA/SMM research examples](docs/RESIDUAL-VMX-BYOVD-DMA-SMM.md) (lab-safe sim only)
- Depth pedagogy library: `code/shared/depth/` + `depth_tests`

## Depth pedagogy (post catalog-99)

- Shared multi-step detectors: `code/shared/depth/` (`ac_depth`)
- Tests: `depth_tests` (ctest)
- Residual research (lab-safe): [docs/RESIDUAL-VMX-BYOVD-DMA-SMM.md](docs/RESIDUAL-VMX-BYOVD-DMA-SMM.md)

## Dummy FPS example

Attacker/defender bomb plant-defuse stand-in: [`code/shared/fps/`](code/shared/fps/) — run `./code/build/fps_demo all`.
