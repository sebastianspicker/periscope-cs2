# T3: Hypervisor-assisted software (rare)

## Intent

Personal hypervisor under (or nested with) Windows so entity data can be read via **guest VA → GPA** translation without classic process handles or a full “RPM driver” visible to simple AC.

## Layer model

```text
L5  Product / UX          radar UI, updater, license
L4  Guest usermode        thin client (auth, draw, request reads)
L3  Guest bridge          weak .sys / VMCALL stub / shared page   ← high signal
L2  Hypervisor core       VMCS/VMCB, EPT, exits, introspection
L1  Boot / load chain     UEFI / early driver / load-order hijack
L0  CPU / firmware        VT-x/AMD-V, often Secure Boot/VBS off
```

## Data path

```text
Game VA (entity)
  → HV walks guest page tables (target CR3)
  → GPA → host-controlled mapping
  → shared buffer / hypercall response
  → radar UI
```

## On-disk / install tree (adversary shape)

See [`../red/t3-hypervisor.md`](../red/t3-hypervisor.md) for full tree.

**Minimal user-facing drop:**

```text
setup.exe | install.bat
hvloader.sys | boot/loader.efi
hvcomm.sys          # bridge
radar.exe
offsets.enc
maps/
README: disable VBS / Hyper-V / Core Isolation
```

## Blue counters (summary)

| Layer | Counter |
|-------|---------|
| L0–L1 | Secure Boot, measured boot, EFI inventory, boot-driver allowlist |
| Policy | Competitive requires VBS + HVCI; score VBS-off |
| L2 | HV presence, timing/CPUID invariants, only trusted virt stack |
| L3 | Bridge driver blocklist, device/IOCTL, VMCALL anomalies |
| L4 | UI co-occurrence, C2 offsets (same as T1) |
| Fallback | Full T0–T2 detectors |
| Design | Fog-of-war; server info-advantage |

## Honest limit

If hostile code owns true VMX root on an untrusted client, client AC can be deceived. Residual trust moves to **attestation**, **server authority**, and **post-hoc demos**.
