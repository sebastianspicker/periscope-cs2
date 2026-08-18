# Red T3: Hypervisor-assisted radar (rare software)

**Goal we counter:** Own CPU virt root; read guest game memory via EPT/GPA; expose a **thin bridge** to a normal-looking radar client. Evade handle graphs and simple “RPM driver” hunts.

## Full project tree (adversary shape)

```text
red-t3-hv-radar/
│
├── README_SUPPORT.md              # disable VBS, Hyper-V, Core Isolation, often SB
├── install.bat | setup.exe
├── uninstall.exe
│
├── boot/                          # L1 load chain
│   ├── loader.efi                 # UEFI path (optional)
│   ├── boot_hook.bin
│   ├── hvloader.sys               # early kernel stub → start HV
│   └── config/
│       └── boot.json
│
├── hypervisor/                    # L2 core
│   ├── hvcore.sys | hvcore.bin
│   ├── arch/
│   │   ├── vmx/                   # Intel: VMCS, VMXON, exit stub
│   │   │   ├── vmx_asm.asm
│   │   │   ├── vmcs.c
│   │   │   └── exit.c
│   │   └── svm/                   # AMD: VMCB
│   │       ├── svm_asm.asm
│   │       └── exit.c
│   ├── mm/
│   │   ├── ept.c                  # identity map, split, hide
│   │   ├── gpa_rw.c
│   │   └── gpt_walk.c             # guest CR3 VA→GPA
│   ├── windows/                   # guest introspection
│   │   ├── koffsets.h             # per Windows build
│   │   ├── process.c              # find game EPROCESS / CR3
│   │   ├── module.c               # client.dll base in guest
│   │   └── pe_guest.c
│   ├── stealth/
│   │   ├── cpuid_spoof.c
│   │   ├── timing.c               # RDTSC / exit-cost hide
│   │   ├── ept_hide.c
│   │   └── nested.c               # interact with Hyper-V if present
│   └── abi/
│       ├── hypercall_ids.h
│       └── handlers/
│           ├── hc_ping.c
│           ├── hc_get_cr3.c
│           ├── hc_read_va.c       # radar fuel
│           ├── hc_write_va.c      # often present even if "radar only"
│           └── hc_protect.c
│
├── bridge/                        # L3 guest-visible — PRIMARY HUNT SURFACE
│   ├── hvcomm.sys
│   ├── hvcomm.inf
│   ├── hvcomm_user.dll            # DeviceIoControl / MapView
│   └── (alt) usermode_vmcall_stub.cpp
│
├── client/                        # L4 — same product face as T1
│   ├── radar.exe
│   ├── src/
│   │   ├── backend/
│   │   │   ├── backend_hv.cpp     # primary
│   │   │   ├── backend_kernel.cpp # T2 fallback
│   │   │   └── backend_rpm.cpp    # T1 fallback
│   │   ├── schema/
│   │   ├── game/
│   │   ├── ui/
│   │   │   └── maps/
│   │   ├── auth/
│   │   └── protect/
│   └── offsets.bin.enc
│
├── tools/                         # often not shipped to end users
│   ├── vbs_check.exe
│   ├── feature_probe.exe          # CPUID / FEATURE_CONTROL
│   ├── offset_dumper.exe
│   └── log_collector.exe
│
├── updater/
│   ├── update.exe
│   ├── channel.ini
│   └── packages/
│       ├── offsets_*.pkg.enc
│       └── client_*.pkg.enc
│
└── c2_contract/                   # operator side
    ├── /api/auth
    ├── /api/offsets
    ├── /api/heartbeat
    └── /api/kill
```

## Minimal deployed footprint (what appears on a gamer PC)

```text
C:\ProgramData\<random>\
├── setup.exe
├── hvloader.sys          # or EFI-only persistence
├── hvcomm.sys            # bridge
├── radar.exe
├── offsets.enc
└── maps\
```

Names/icons often disguised as RGB, VPN, or “system utility.”

## Runtime topology

```text
[CPU VMX root: hvcore]
       ^  hypercalls / shared GPA
       |
[hvcomm.sys]  <--- IOCTL --->  [radar.exe] ---> UI / phone
       |
       |  GPA walk / EPT
       v
[Windows guest: game.exe memory]
```

## Hypercall ABI surface (treat as red API)

| Op | Role | Blue angle |
|----|------|------------|
| PING | Confirm “our” HV | Timing + unexpected HV vendor |
| GET_CR3 / FIND_PROCESS | Target selection | Bridge IOCTL patterns |
| READ_VA | Entity fuel | High-value; pair with UI process |
| WRITE_VA | Feature creep | Game integrity if used |
| EPT_PROTECT | Hide pages | AC self-integrity from higher trust |

## Implementation strategy (condensed)

1. Force or instruct **VBS/Hyper-V off** (or nest — rarer).
2. Load **L1** early enough to virtualize the running OS.
3. Build **EPT**, intercept CPUID (and often timing).
4. Expose **L3 bridge** so L4 can request reads.
5. Ship **L4 radar** identical in UX to T1.
6. Keep **T2/T1 fallbacks** when HV init fails.
7. Encrypt offsets; polymorphic client builds.

## Blue mapping (T3-specific)

| Red node | Blue control |
|----------|----------------|
| `README` disable VBS | Policy gate: ranked requires VBS/HVCI |
| `boot/` `hvloader` | Boot driver allowlist, ELAM, EFI inventory |
| `hypervisor/` | HV presence, timing, attestation |
| `bridge/hvcomm.sys` | Driver intel, device/IOCTL, blocklist |
| `client/backend_hv` | Correlate UI ↔ bridge |
| `backend_*` fallbacks | Full T0–T2 stack |
| Entity data existence | Fog-of-war / interest management |
| Clean client demos | Server info-advantage scoring |

→ [`../blue_shapes/project-tree.md`](../blue_shapes/project-tree.md)
→ [`../../2026-07-21-pre-comprehensive/T3-HYPERVISOR.md`](../../2026-07-21-pre-comprehensive/T3-HYPERVISOR.md)
