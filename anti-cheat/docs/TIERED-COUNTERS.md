# Tiered counter strategies

Red does X; blue counters with Y. Software-first. DMA residual at T4.

## Quick matrix

| Tier | Red core | Blue primary | Blue structural / residual |
|------|----------|--------------|----------------------------|
| T0 | Usermode RPM + external UI | Handle graph, process co-occurrence | Fog, info-advantage |
| T1 | Syscall RPM / packed soft | Same handles; ignore API-hook blindness | Same + lineage / ETW residual |
| T2 | Kernel or BYOVD (no handle) | Driver load, blocklist, device/IOCTL | Callback integrity; T0-T1 fallbacks |
| T3 | Personal HV + thin bridge | Trust policy (VBS/HVCI), HV probe, bridge | Attestation, dual-view, server authority |
| T4 | DMA / second PC / capture residual | IOMMU / device policy (limited) | Fog, info-advantage, multi-signal overwatch |

## T0: Usermode external

| Priority | Blue strategy | How (lab) |
|----------|---------------|-----------|
| 1 | Handle graph | Foreign VM_READ on game PID |
| 2 | Process co-occurrence | Session co-run with reader names/parents |
| 3 | Window / capture heuristics | Overlay, GDI/PrintWindow, BitBlt (weak alone) |
| 4 | Reputation / allowlist care | Reduce false positives; never sole allowlist |
| 5 | Info-advantage | Pre-aim without vision or sound |
| 6 | Interest management | Do not send unobservable enemy XY |

## T1: Syscall soft

| Priority | Blue strategy | How (lab) |
|----------|---------------|-----------|
| 1 | Same handle graph | Syscall open still creates handle scar |
| 2 | Do not require ntdll hooks | Hook absence is not clean |
| 3 | Parent lineage / stub loaders | Short-lived parents, mapper artifacts |
| 4 | Stack / ETW residual | Spoof and blind are multi-step scars |
| 5 | Structural | Fog + info-advantage unchanged |

## T2: Kernel / BYOVD

| Priority | Blue strategy | How (lab) |
|----------|---------------|-----------|
| 1 | Driver image / hash blocklist | byovd_known_bad, signer, load order |
| 2 | Device / IOCTL surface | memrw devices, suspicious names |
| 3 | Callback integrity | Process/object notify baselines, shadow |
| 4 | SCM / early load race | Boot-start services before AC |
| 5 | Keep T0-T1 live | Packs fall back to RPM |

## T3: Hypervisor (sim)

| Priority | Blue strategy | How (lab) |
|----------|---------------|-----------|
| 1 | Ranked trust policy | Require VBS+HVCI (and SB/attest as product dictates) |
| 2 | Multi-invariant HV probe | Vendor, latency, personal HV, timing spoof |
| 3 | Bridge intel | is_bridge driver/device/channel |
| 4 | Attestation / dual-view | PCR fail, EPT hide vs SK dirty |
| 5 | Residual flags | CI options, feature control MSR, infinity hook, VTL1 miss, EFI/ELAM |
| 6 | Structural | Fog still wins if client ever had free XY |

## T4: DMA residual (sim)

| Priority | Blue strategy | How (lab) |
|----------|---------------|-----------|
| 1 | IOMMU / ranked device policy | Weak; OEM-dependent |
| 2 | Interest management | Primary; client never had far enemy origin |
| 3 | Info-advantage / overwatch | Human radar use still leaks |
| 4 | Capture / clone / lag residuals | Correlate multi-sensors, not one bool |

## Crosscutting counters (any tier)

| Red theme | Blue theme | Example IDs |
|-----------|------------|-------------|
| Staged loader / manual map | Runtime RX + C2 | 12_staged_loader |
| Poly build / watermark | Handle + extractable wm | 75_, 87_ |
| HWID spoof | Payment/IP graph | 15_, 23_, 86_ |
| Soft aim | Snap slips; residual IA | 11_aim_humanization |
| Silent aim | Camera vs server aim | 62_silent_aim_desync |
| Fallback HV to kernel to RPM | Stack all detectors | 20_fallback_chain |
| Stream crypto with client key | Strip key + fog | 40_entity_stream_crypto |
| Weak multi-scars | Delayed ban / overwatch score | 26_, 74_, 98_ |

## Fallback rule

```text
If blue only monitors the highest tier claimed:
  red degrades one step and walks free.

Always run T0 handle + T2 device + T3 trust + server residual together.
```
