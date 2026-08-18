# Red T2: Kernel / BYOVD external radar

**Goal we counter:** Same radar product UX as T1, but memory reads happen in **kernel** so the UI process often has **no open handle** to the game.

## Project tree (adversary shape)

```text
red-t2-kernel-byovd-radar/
│
├── client/                              # looks like T1
│   ├── radar.exe
│   ├── src/
│   │   ├── ui/ ...
│   │   ├── schema/ ...
│   │   ├── game/entity_list.cpp         # same logic, different read API
│   │   └── backend/
│   │       ├── iface.h                  # read(va, size) → bytes
│   │       ├── winapi_rpm.cpp           # fallback T0/T1
│   │       ├── kernel_ioctl.cpp         # DeviceIoControl → .sys
│   │       └── byovd_ioctl.cpp          # IOCTL to vulnerable signed .sys
│   ├── auth/
│   └── protect/
│
├── driver-custom/                       # private unsigned/test-signed path
│   ├── hv_or_mem.sys.vcxproj
│   ├── src/
│   │   ├── entry.c                      # DriverEntry, device create
│   │   ├── ioctl.c                      # READ_VA / READ_PROCESS
│   │   ├── copy.c                       # MmCopyVirtualMemory / attach
│   │   ├── hide.c                       # optional: strip notifies (aggressive)
│   │   └── comm_abi.h                   # ioctl codes + req structs
│   ├── inf/
│   │   └── memrw.inf
│   └── sign/                            # leaked/test certs in wild packs
│
├── byovd/                               # alternative to custom driver
│   ├── vulnerable_signed.sys            # known-bad allowlisted-by-age drivers
│   ├── loader_service.exe               # SCM install/start
│   ├── exploit_wrapper.cpp              # map physical / arbitrary RW primitives
│   └── blocklist_evasion notes          # rename, delay load, etc.
│
├── install/
│   ├── setup.exe
│   ├── disable_dse_or_testsigning.md    # or "mapper" path
│   └── service_name_randomizer
│
├── updater/                             # offsets + sometimes new .sys hash
│
└── c2/
    ├── /api/auth
    └── /api/offsets
```

## Runtime topology

```text
[radar.exe]
    |  DeviceIoControl / mapped API
    v
[memrw.sys | vulnerable.sys] --kernel read--> [game process pages]
    |
    v
[UI / WebSocket]  (no OpenProcess from radar.exe in clean design)
```

## Implementation strategy

| Mode | Strategy | Primary blue counter |
|------|----------|----------------------|
| **Custom driver** | IOCTL read engine owned by cheat | Image load notify, hash/cert, device object, service |
| **BYOVD** | Abuse signed vulnerable driver IOCTLs | **Cloud blocklist** of known-bad drivers; load alerts |
| **Mapper** | Load unsigned via vulnerable bring-up | Same + mapper usermode artifacts |
| **Fallback** | Drop to T1 RPM if driver fails | Handle graph still required |

## Aggressive T2 extras (expect in private packs)

```text
driver/src/hide.c
  - tamper process/image notify lists (where still viable)
  - obscure device names
  - brief device lifetime
```

## Blue mapping

→ Driver allow/block lists, SCM telemetry, BYOVD intel  
→ Still run full T0–T1 detectors (fallback)  
→ [`../../matrices/RED-BLUE-MAP.md`](../../matrices/RED-BLUE-MAP.md) rows T2
