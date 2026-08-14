# 110_hwid_spoof - Hardware Identity Spoofing

Family: Evasion. Scope: `sim::World` only.

## Context

Ban evasion attempts can seek to rotate identifiers reported by network adapters, disks, volumes, firmware, or hardware-backed identity services. Individual values are not reliable identity proof: they can change legitimately, be virtualized, or be privacy-protected.

## Lab

Red changes only a simulated composite identity string and records two account observations with a stable payment fingerprint. It does not query or modify MAC addresses, disks, the registry, firmware, drivers, TPMs, or the operating system.

Blue flags a deliberately inconsistent profile: disk and MAC components change while SMBIOS and account linkage remain stable. A deny decision requires all three signals.

## Takeaway

Use privacy-reviewed, server-side correlation across independent sources and treat mismatches as risk signals, not conclusive proof from any one hardware identifier.
