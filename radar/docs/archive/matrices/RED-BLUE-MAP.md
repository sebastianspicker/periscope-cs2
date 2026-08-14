# Red → Blue map

Each red component maps to blue owners and primary signals.

## T0–T1 usermode external

| Red path / component | Strategy | Blue owner | Primary signal |
|----------------------|----------|------------|----------------|
| `process/attach` | Find game PID | `process_watch` | Game + reader co-run |
| `rpm_backend` | `OpenProcess` + RPM | `handle_graph` | VM_READ handle on game |
| `syscall_backend` | Direct syscall RVM | `handle_graph` (+ ETW) | Same handle |
| `schema/offsets*` | Survive patches | `threat_intel` / updater watch | Weak alone |
| `game/entity_list` | Parse client state | `interest_mgmt` | **Remove data** |
| `ui/window_*` | 2nd window radar | `window_watch` | Top-level window class |
| `ui/web` | Phone radar feed | `network_watch` | Local WS / SaaS |
| `loader/stub` | Stage payload | `process_watch` | Reflective regions |
| `protect/*` | Break YARA | — | Don’t rely on static |
| `auth/hwid` | Seat license | `ban` / intel clusters | Repeat offenders |

## T2 kernel / BYOVD

| Red path / component | Strategy | Blue owner | Primary signal |
|----------------------|----------|------------|----------------|
| `driver-custom` | `MmCopyVirtualMemory` IOCTL | `driver_guard` | Unknown .sys + device |
| `comm_abi` / IOCTL | Usermode ↔ kernel | `device_watch` | Open device from UI proc |
| `byovd/vulnerable_signed.sys` | Signed primitive | `blocklist` | Known-bad load |
| `byovd/loader_service` | SCM install | `image_notify` + SCM | Service create |
| `hide.c` | Tamper callbacks | `callback_audit` | Notify chain integrity |
| `backend fallbacks` | Drop to RPM | `handle_graph` | Still required |

## T3 hypervisor

| Red path / component | Strategy | Blue owner | Primary signal |
|----------------------|----------|------------|----------------|
| Support: disable VBS | Clear field for HV | `policy` / `attestation_gate` | VBS/HVCI off at ranked |
| `boot/loader.efi` | Early control | trust / EFI inventory | Unexpected boot entry |
| `boot/hvloader.sys` | Start HV | boot-start allowlist | Unknown boot driver |
| `hypervisor/ept + gpt_walk` | Hidden reads | `hv_probe` + attestation | HV present / timing |
| `stealth/cpuid_spoof` | Hide HV bit | `hv_probe` multi-invariant | Inconsistent CPUID/timing |
| `bridge/hvcomm.sys` | Channel to UI | `driver_guard` | Bridge device + IOCTL |
| `abi/hc_read_va` | Radar fuel | correlate bridge + UI | Read channel active |
| `client/backend_hv` | Product face | `process_watch` | UI without handles |
| `client/backend_*` fallback | T2/T1 | full lower stack | Handles / BYOVD |
| Entity bytes exist | Client trust | `interest_mgmt` | Structural kill |
| Human radar use | No aimbot | `info_advantage` | Demo features |

## Cross-cutting

| Red ops | Blue |
|---------|------|
| Polymorphic builds | Behavior + graph, not hash-only |
| Offset C2 | Intel optional; ban correlator |
| HWID spoofers | Cluster + payment/IP; soft HWID |
| Delayed “legit” play | Delayed bans + overwatch |
