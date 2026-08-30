# thread_monitor_evade — Thread Start Address Monitoring

Family: Detection. Tiers: T1. Area: cs2 diagnostics.

## Battlefield
Red: Records a thread start address inside loaded `kernel32.dll`.
Blue: Models `NtQueryInformationThread(ThreadQuerySetWin32StartAddress)` and flags
RWX, unmapped, suspicious-module, and system-trampoline starts.

## Takeaway
Being inside a loaded module is insufficient by itself: the simulation flags a
system-DLL trampoline start even though its address is mapped and executable.
