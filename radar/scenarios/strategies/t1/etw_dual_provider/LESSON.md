# etw_dual_provider — ETW Dual Provider Counter-Blinding

Family: Evasion. Tiers: T1-T2.

## Battlefield
Red: Blinds the primary ETW Threat Intelligence provider (Microsoft-Windows-Threat-Intelligence)
Blue: Has a SECONDARY, redundant ETW provider that survives TI blinding

## Real AC Context
CS2/VAC use MULTIPLE independent ETW providers:
- Microsoft-Windows-Threat-Intelligence (primary, well-known)
- Microsoft-Windows-Kernel-Process (thread creation events)
- Microsoft-Windows-Security-Auditing (handle creation events)
- Custom ETW providers registered by the AC itself

If red blinds only TI, the other providers still capture:
- Thread creation (Thread start address)
- Handle creation (OpenProcess with VM_READ)
- Module load events

Blinding ALL providers is possible but requires repeated driver-level operations
that create their OWN forensic footprint.

## Lesson
Single-provider blinding is insufficient. Blue should maintain REDUNDANT
telemetry sources. Red must blind ALL providers or accept detection by
the secondary ones.
