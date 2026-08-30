# vac_handle_enum — VAC Handle Table Enumeration

Family: Detection. Tiers: T0-T1. Area: t0_blue.

## Battlefield
Red: Opens a PROCESS_VM_READ handle to cs2.exe
Blue: Scans the system handle table via NtQuerySystemInformation(SystemHandleInformation)

## Real AC Context
VAC (via steam.exe) scans all handles system-wide. Any PROCESS_VM_READ handle
pointing to cs2.exe from a non-Steam PID is flagged. This is the PRIMARY
detection vector against external T0 RPM tools.

## Dual mode
SIM: Handles are simulated in sim::World handle table
REAL: Real handle is visible in the system handle table (CS2 must be running)
