# Thread summary

Research conversation on anti-cheat vs sophisticated software cheats, refined to **external legit radar** and tiered software stacks (no DMA focus).

## Session arc

1. **Landscape (2026)** — Cheats stack across usermode → kernel → hypervisor → hardware. High-end products combine read-only memory, obfuscation, humanization, and offset automation. Durable defense: reduce client trust, attest environment, continuous integrity, behavior + social graph, delayed forensic bans.

2. **External legit radar focus** — Dominant “looks legit” class: information only (top-down map / phone radar), no aim write, often no inject, no game `.text` patch. Root cause: client already holds enemy positions. Backend ladder: WinAPI RPM → indirect syscalls → kernel R/W → BYOVD → (rare) HV → (out of scope) DMA.

3. **Software-only expectation** — Users lack DMA budget → expect **code**:
   - **Median:** T1 paid soft external (RPM/syscall + separate window/web UI).
   - **Hard common:** T2 kernel/BYOVD no-handle radar.
   - **Rare:** T3 hypervisor-assisted read.

4. **T3 elaboration** — Structure: boot/load chain → HV core (EPT + guest VA walk) → thin guest bridge → normal radar client → encrypted offsets/C2. Primary counters: VBS/HVCI/Secure Boot policy, who owns VMX/SVM, bridge driver/hypercall ABI, T1/T2 fallback paths, server fog-of-war + info-advantage demos.

## Key conclusions

| Topic | Conclusion |
|-------|------------|
| Why radar | Info advantage without aimbot kinematics or inject signals |
| Why “undetected” marketing | Read-only + no overlay + kernel path empties classic signals |
| What encryption does | Protects offsets/business; does **not** hide handles/drivers/HV |
| What static YARA does | Weak against per-user polymorphic builds |
| Best structural fix | Interest management / don’t send unobservable enemy XY |
| Best client signals (software) | Handle graph (T0–T1), driver/BYOVD (T2), HV+bridge+trust (T3) |
| Best server signals | Pre-aim / track without vision or sound; delayed bans |
| DMA | Deprioritized for this userbase |

## Adversary default model (this research)

```text
Paid/free external EXE
  → WinAPI or syscall read  OR  kernel/BYOVD
  → Encrypted offsets from seller C2
  → SFML/ImGui window OR localhost web → phone
  → Light packer / unique build
  → No aimbot, often no game inject
  → Optional HWID spoofer after ban
  → Rare: personal HV under Windows + bridge.sys
```

## Blue priority order (agreed in thread)

| P | Focus |
|---|--------|
| P0 | Usermode RPM → **handle graph** |
| P1 | Syscall RPM → same handles; less trust in API hooks |
| P2 | Kernel / BYOVD → driver blocklist + load telemetry |
| P3 | Web/phone UI → local reader still P0–P2 + network/SaaS |
| P4 | Behavior-only clean env → server info-advantage |
| P5 | Design → fog-of-war / interest management |
| T3 | Trust policy + HV presence + bridge + attestation |

## Explicit non-goals (this folder)

- Working cheat / HV / BYOVD implementations
- Exploit PoCs against production AC
- DMA lab procedures (unless later scoped)

## Explicit goals

- Shared vocabulary (T0–T3)
- Red trees as **counter-target maps**
- Blue trees as **engineering blueprint**
- Matrices for tickets and red-team tests
