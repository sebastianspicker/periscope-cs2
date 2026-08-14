# vas_walk_evade — VAS Walk Baseline Evasion

Family: Evasion. Tiers: T1-T2.

## Battlefield
Red: Avoids detection by VAC's memory scanner. VAC enumerates ALL
     committed pages at startup (baseline), then polls for changes.
Blue: VAC's VAS walk enumerates committed pages via NtQueryVirtualMemory.
      Pages committed AFTER the baseline are flagged.

## Real AC Context
VAC's external scanner (steam.exe):
1. At process start: calls NtQueryVirtualMemory on cs2.exe
   → records all committed pages (~150+ module descriptors)
2. Continuously polls module descriptors (~136 bytes each)
3. Any NEW committed page (not in baseline) is flagged as suspicious

This is why manual mapping and late code allocation are detectable:
the new page has no corresponding module entry in the VAS walk baseline.

## Technique
Red MUST pre-allocate all memory pages before VAC takes its baseline:
- Allocate all expected pages at DLL load time (DLL_PROCESS_ATTACH)
- Use the same page protection as legitimate modules (RX, not RWX)
- Never allocate additional pages after the baseline is taken

Pre-allocation means red must know its memory requirements AHEAD of time.

## Lesson
Late allocation is detected. Pre-allocation avoids VAS walk detection.
This is why some cheats pre-allocate large "spray" regions at startup.
