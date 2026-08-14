# proxy_hijack_reader — Handle Graph Proxy/Hijack Reader

Family: Evasion. Tiers: T0.

## Battlefield
Red: Radar process never opens a handle. A separate proxy process (that
     legitimately has a game handle) performs reads on its behalf.
Blue: Scans handle table for VM_READ handles to game. Finds none from
     radar process.

## Real AC Context
This is the model described in VAC architecture documents as the safest
approach for external tools. VAC's primary detection vector is handle
table enumeration (NtQuerySystemInformation). If no handle exists from
the reader process, this vector is completely blind.

## Mechanics
1. Proxy opens handle to game (same process that launched it, or one
   with legitimate reason for the handle)
2. Radar sends read request via IPC (named pipe, shared memory)
3. Proxy performs ReadProcessMemory, returns results
4. Radar receives entity data, renders overlay
5. Radar has zero handles → invisible to handle table scan

## Dual mode
SIM: Handle table is simulated in sim::World
REAL: Two processes would be needed (educational concept only)
