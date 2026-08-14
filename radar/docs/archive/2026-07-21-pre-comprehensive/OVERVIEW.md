# Overview

## Problem

External “legit” radar reads client entity data and shows a top-down map.  
No aimbot required → hard for pure aim ML. Software stack escalates T0→T4.

## Tiers (spine of this repo)

| Tier | Folder | Scar red leaves | Blue primary |
|------|--------|-----------------|--------------|
| **T0** | `code/tiers/t0_usermode_rpm` | Usermode VM_READ handle | Handle graph |
| **T1** | `code/tiers/t1_syscall_soft` | Same handle, syscall path | Handles (not ntdll hooks) |
| **T2** | `code/tiers/t2_kernel_byovd` | Driver / BYOVD / device | Blocklist + device watch |
| **T3** | `code/tiers/t3_hypervisor` | VBS-off + HV + bridge | Trust policy + probe |
| **T4** | `code/tiers/t4_dma_residual` | Often no local process | IOMMU + fog + demos |

Each tier folder:

```text
tiers/tN_*/
├── LESSON.md
├── README.md
├── red/           # libraries
├── blue/
├── strategies/    # atomic red↔blue lessons for this tier
└── demos/         # duel + optional protos
```

## Cross-cutting (not a delivery tier)

| Folder | Contents |
|--------|----------|
| `crosscutting/evasion` | staging, packer, offsets C2, HWID |
| `crosscutting/features` | overlay, phone radar, humanization, input synth |
| `crosscutting/structural` | fog-of-war, info-advantage, delayed ban, fallbacks |
| `crosscutting/ops` | account graph, C2 intel, input provenance |

## Escalation

```text
T0 RPM ──► T1 syscall ──► T2 kernel/BYOVD ──► T3 HV ──► T4 DMA
                              ▲
                    crosscutting evasion on any tier
                              ▼
              structural controls kill value even if scars miss
```

## Placement rule

| Question | Put under |
|----------|-----------|
| How is memory read? | `tiers/tN/strategies/` |
| How is the same backend hidden? | `crosscutting/evasion/` |
| Product feature on any backend? | `crosscutting/features/` |
| Server/policy/graph? | `crosscutting/structural` or `ops` |

## Further reading

- [THREAD-SUMMARY.md](THREAD-SUMMARY.md)
- [THREAT-TIERS.md](THREAT-TIERS.md)
- [TIERED-COUNTERS.md](TIERED-COUNTERS.md)
- [code/tools/strategy_lab/CATALOG.md](../code/tools/strategy_lab/CATALOG.md)
