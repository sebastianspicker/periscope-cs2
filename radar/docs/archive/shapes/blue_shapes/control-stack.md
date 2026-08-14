# Blue control stack

Ordered by leverage for **software external radar** (DMA deprioritized).

## P0 — Handle graph (T0–T1)

| Item | Detail |
|------|--------|
| **Detects** | Foreign process with `PROCESS_VM_READ` / related access on game |
| **Blue modules** | `handle_graph/`, object callbacks |
| **FP notes** | Debuggers, overlays, RGB software, recs — need allowlist + reputation |
| **SLO** | Detect within one match for naive RPM |

## P1 — Syscall RPM (T1)

| Item | Detail |
|------|--------|
| **Detects** | Same handles; path avoids hooked ntdll |
| **Blue modules** | Same as P0; optional ETW/syscall telemetry |
| **Do not** | Rely only on usermode API detours |

## P2 — Kernel / BYOVD (T2)

| Item | Detail |
|------|--------|
| **Detects** | Custom R/W drivers; known vulnerable drivers; suspicious devices |
| **Blue modules** | `driver_guard/`, `image_notify`, `blocklist_publish` |
| **Ops** | Fast cloud blocklist updates after new BYOVD waves |
| **FP notes** | Legitimate anti-cheat/tools drivers — allowlist carefully |

## P3 — UI / web / phone (supportive)

| Item | Detail |
|------|--------|
| **Detects** | Radar window, localhost WS, known SaaS domains |
| **Blue modules** | `window_watch`, `network_watch` |
| **Limit** | Easy to hide (phone + minimal PC footprint) |

## P4 — Info-advantage behavior (all tiers, clean env)

| Item | Detail |
|------|--------|
| **Detects** | Knowledge without vision/sound (pre-aim, wall track, perfect rotate) |
| **Blue modules** | `server/info_advantage`, demos, overwatch |
| **Requires** | Server-side visibility & audio events aligned to timeline |

## P5 — Interest management (structural)

| Item | Detail |
|------|--------|
| **Mitigates** | All memory radars: no data → no blips |
| **Blue modules** | `server/interest_mgmt` |
| **Tradeoffs** | Netcode, peeker’s advantage, AI, demos |

## T3 — Trust & hypervisor pressure

| Item | Detail |
|------|--------|
| **Policy** | Ranked requires VBS + HVCI (+ Secure Boot as product allows) |
| **Probe** | CPUID/timing/HV vendor; unexpected virt |
| **Bridge** | Treat like T2 drivers (`hvcomm`-class) |
| **Attestation** | TPM/Secure Launch when available |
| **Limit** | Hostile root HV on unlocked machine → server + policy residual |

## Correlation & response

```text
signals → risk_score → (soft: challenge / demo flag) → delayed_ban
                              ↘ immediate only on high-confidence (e.g. known BYOVD + active read device)
```

## What not to over-index

- Game `.text` integrity alone (externals don’t patch it)
- Aimbot-only ML (radar-only users miss)
- Public YARA of free radars (private rebuilds)
- Overlay hooks alone (legit radar avoids game overlay)
