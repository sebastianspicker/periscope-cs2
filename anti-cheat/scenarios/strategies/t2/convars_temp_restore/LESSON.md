# convars_temp_restore — ConVar Temporary Modification + Restoration

Family: Evasion. Tiers: T2.

## Battlefield
Red: Modifies a ConVar (e.g., sv_cheats), performs its technique, then restores
     the original value BEFORE blue's ConVar CRC check runs.
Blue: Collects CRC32 of all ConVars (Message 157) at collection time. If the
     CRC doesn't match the server's expected CRC, flags as tampered.

## Real AC Context
CS2's Message 157 (UtilAction) computes two CRC32 values:
  - crc_all: CRC32 of ALL ConVar name+flag pairs
  - crc_replicated: CRC32 of FCVAR_REPLICATED ConVars only

If the server's expected CRC differs from what's collected, the client is
flagged. Temporary modification with restoration avoids this by ensuring
the CRC matches at collection time.

## Technique
1. Red saves ConVar original value
2. Red modifies ConVar (e.g., sv_cheats 1 to see radar always)
3. Red performs technique (reads entity positions, etc.)
4. Red RESTORES original ConVar value
5. Blue collects ConVar CRC → matches expected → no detection

## Dual mode
SIM: Uses sim::World ConVar state with CRC collection timing
REAL: Would modify and restore CS2 ConVars via memory write
