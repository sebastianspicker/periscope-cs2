# norm_hash_evade — Normalized Module Hash Evasion

Family: Evasion. Tiers: T1.

## Battlefield
Red: Modifies a module's code section (for hooks, detours). The change alters
     the normalized section hash.
Blue: VAC's normalized module hashing: reads the module from DISK, normalizes it
     (undo relocations, zero EAT/IAT), computes CRC32/SHA1 over read-only sections.
     Any mismatch between the in-memory and on-disk hash is flagged.

## Real AC Context
VAC scanner:
1. Reads the module from disk (file)
2. Normalizes: undo relocations, zero EAT, zero IAT
3. Computes SHA1/CRC32 over .text, .rdata sections
4. Compares against known-good hash

If red modifies a module's code (hooks, patches), the hash changes.
Red must either:
- Restore the original code before the scanner runs
- Use a module that VAC doesn't scan (e.g., manually mapped)
- Modify only data sections (not .text or .rdata)

## Technique
Red should:
- Use hardware breakpoints (dr0-dr3) instead of code patches
- Use VEH (Vectored Exception Handling) instead of hooks
- Only modify writable sections (.data), never .text or .rdata

## Lesson
Code section modifications change the normalized hash. VAC catches this.
Use alternative hooking techniques that don't modify scanned sections.
