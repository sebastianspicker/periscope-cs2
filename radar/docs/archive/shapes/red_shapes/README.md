# Red project structures (adversary — what we counter)

These trees describe **how adversary software is organized**, so blue can map detectors and tests.  
They are **architecture maps**, not a build guide. Do not implement working cheats here.

## Index

| Doc | Tier | One-line |
|-----|------|----------|
| [`t0-t1-usermode-external.md`](t0-t1-usermode-external.md) | T0–T1 | Separate process, RPM/syscall, external UI |
| [`t2-kernel-byovd.md`](t2-kernel-byovd.md) | T2 | Kernel/BYOVD read, no game handle |
| [`t3-hypervisor.md`](t3-hypervisor.md) | T3 | Personal HV + bridge + thin client |

## Shared red pipeline (all software radar tiers)

```text
[Resolve game PID / CR3]
        →
[Read backend: RPM | syscall | kernel | BYOVD | HV]
        →
[Schema: bases + offsets / patterns]
        →
[Entity walk: origin, team, life, map]
        →
[Project to 2D radar space]
        →
[UI: window | web | phone]
        →
[Ops: auth, HWID, updater, obfuscation]
```

## Shared red ops modules (expect in every paid pack)

```text
red-product/
├── loader/          # auth, staging, anti-debug
├── protect/         # packer, string encrypt, polymorphism
├── offsets/         # encrypted schema + post-patch updates
├── backend/         # tier-specific read engine
├── features/radar/  # entity → blips
├── ui/              # draw or stream to phone
└── c2/              # license, offsets, kill-switch
```

## Blue rule of thumb

| If red has… | Blue owns… |
|-------------|------------|
| `backend/rpm` | Handle graph |
| `backend/syscall` | Handle graph + less API-hook trust |
| `backend/kernel` / `byovd` | Driver load + blocklist + devices |
| `backend/hv` + `bridge` | Trust policy + HV signals + bridge |
| `ui` only | Weak alone; pair with backend |
| `c2/offsets` | Network/intel; not sufficient alone |
| Any tier | Server fog-of-war + info-advantage |
