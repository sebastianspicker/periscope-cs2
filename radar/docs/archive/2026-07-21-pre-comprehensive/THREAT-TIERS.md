# Threat tiers (software-first)

Hardware DMA/FPGA is noted only as residual risk above T3 software.

## Tier table

| Tier | Name | Read path | Handle to game? | Typical UI | Prevalence (assumed userbase) |
|------|------|-----------|-----------------|------------|-------------------------------|
| **T0** | Free/public external | `OpenProcess` + `ReadProcessMemory` | Yes | SFML / ImGui / Win32 | High |
| **T1** | Paid soft external | RPM or **indirect syscalls** | Yes | 2nd window / browser / phone feed | **Median** |
| **T2** | Private “UD” software | **Kernel driver** or **BYOVD** R/W | Often **no** usermode handle | Same as T1 | Paid minority |
| **T3** | Rare HV software | Personal **hypervisor** introspection | No (via HV) | Same + thin bridge | Rare |
| **T4*** | Hardware | PCIe DMA / 2nd PC | N/A | 2nd PC / FPGA LCD | Out of primary scope |

\*Not a focus for this research folder.

## Capability by tier

| Capability | T0 | T1 | T2 | T3 |
|------------|----|----|----|-----|
| Entity radar from client memory | ✓ | ✓ | ✓ | ✓ |
| No game inject | ✓ | ✓ | ✓ | ✓ |
| No game write | ✓ | ✓ | ✓ | ✓ |
| Evade simple usermode API hooks | | partial | ✓ | ✓ |
| Evade handle-graph detectors | | | often | ✓ |
| Evade naive “no unknown .sys” | | | BYOVD blurs | custom HV |
| Evade VBS/HVCI-required ranked | | | hard | hard |
| Worthless if fog-of-war strong | ✓ | ✓ | ✓ | ✓ |

## Default adversary (engineering target)

Design blue controls so that:

1. **T0–T1** are reliably detected within a match (handles + process co-occurrence).
2. **T2** is detected via driver/BYOVD policy within session or at boot.
3. **T3** is constrained by **trust policy** (cannot play competitive without VBS/HVCI) and residual HV/bridge signals.
4. **All tiers** lose value under **server interest management**.
5. **All tiers** remain accountable via **info-advantage** demo scoring when client is “clean.”

## Degradation pattern (important)

Many commercial packs implement:

```text
try T3 HV → else T2 kernel/BYOVD → else T1 RPM
```

Blue must run **all lower-tier detectors even when hunting T3**.
