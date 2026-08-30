# Overview

## Problem

External "legit" radar reads client-held entity data and draws a top-down map or phone feed. It avoids aimbot writes, inject, and game text patches. Aim-only ML does not catch it. Classic inject and YARA sensors miss clean readers. Delivery stacks escalate T0 through T4 as blue closes lower scars.

This lab is a tier-first, sim-backed training ground for that adversary class and its counters (radar track under the Periscope monorepo).

## What this lab is

| Is | Is not |
|----|--------|
| Educational red/blue exercises on `sim::World` | Production product or cheat pack |
| Multi-step strategy catalog + T0–T4 team libs | Single-bool flag detectors as the quality bar |
| Multi-sensor blue (graph, trust, fog, behavior) | Drop-in commercial AC replacement |
| Headless C++ demos and `strategy_lab` | Full 3D game client |
## Delivery tiers (spine)

| Tier | Folders | Red leaves | Blue primary |
|------|---------|------------|--------------|
| T0 | src/lab_components/teams/t0_red/ + src/lab_components/teams/t0_blue/ | Usermode VM_READ handle | Handle graph, co-occurrence |
| T1 | src/lab_components/teams/t1_red/ + src/lab_components/teams/t1_blue/ | Same handle, syscall-shaped path | Handles (ignore ntdll hooks) |
| T2 | src/lab_components/teams/t2_red/ + src/lab_components/teams/t2_blue/ | Driver, BYOVD, device IOCTL | Blocklist, device watch, callbacks |
| T3 | src/lab_components/teams/t3_red/ + src/lab_components/teams/t3_blue/ | VBS/HVCI off, personal HV, bridge | Trust policy, HV probe, bridge, attest |
| T4 | src/lab_components/teams/t4_red/ + src/lab_components/teams/t4_blue/ | DMA device, often no local process | IOMMU policy, fog, info-advantage |

Each tier ships:

```text
src/lab_components/teams/tN_red/  src/lab_components/teams/tN_blue/          team code
scenarios/strategies/<tier>/                     strategy lessons (all tiers + crosscutting)
apps/demos/duel_tN.cpp  apps/demos/proto_tN_*.cpp  apps/demos/cs2_radar/tN/
docs/tiers/tN/                        LESSON.md + README.md
```

## Crosscutting (not a delivery tier)

| Area | Folder | Role |
|------|--------|------|
| Evasion | `scenarios/strategies/` (evasion-tagged pairs) | Staging, crypto, C2 offsets, HWID, poly builds, canaries |
| Features | `scenarios/strategies/` (feature-tagged pairs) | Overlay ESP, phone radar, humanization, input synth, silent aim |
| Ops | `scenarios/strategies/` (ops-tagged pairs) | Account graph, reports, VPN, C2 intel, overwatch scores |
| Structural | `scenarios/strategies/` (structural-tagged pairs) | Fog, info-advantage, delayed ban, stream crypto, fallbacks |

## Design conclusions

1. Radar sells info advantage without aimbot kinematics — that is why it persists.
2. "Undetected" claims stick when blue only looks for injects and YARA while T1/T2 empty those signals.
3. Offset encryption and packers protect the binary; they do not erase handle, driver, or HV scars.
4. Static YARA loses to per-buyer polymorphic builds. Blue has to hunt runtime scars.
5. Best blue win across tiers: server interest management (do not send free far-enemy origins).
6. When the client looks clean, info-advantage scoring and delayed multi-signal restrictions are the residual levers.

## Escalation (adversary)

```text
T0  OpenProcess + RPM + external UI
    blue: handle graph
T1  Syscall open/read (bypass usermode hooks)
    blue: same handle graph (hooks are theater)
T2  Kernel / BYOVD / custom memrw device
    blue: driver policy, device watch, callbacks
T3  Personal HV + thin bridge (VBS/HVCI off)
    blue: ranked trust policy + HV probe + bridge
T4  Off-box DMA / capture residual
    blue: IOMMU (weak) + fog + behavioral residual
```

## Degradation (product packs)

Real packs fall back: HV fails, then kernel, then usermode RPM. Blue must stack all lower sensors, not only hunt the highest tier (strategy 20_fallback_chain).

## Multi-step quality bar (this codebase)

- Red: at least two related scars or a multi-phase plant (not one bool flip).
- Blue: multi-reason correlation or score threshold (not echo one flag).
- Tests: call shipped entry points (examples::*, tN::, depth::, server::) on fresh arenas.

## Start learning

See CURRICULUM.md, ARCHITECTURE.md, and BUILD-AND-TEST.md.
