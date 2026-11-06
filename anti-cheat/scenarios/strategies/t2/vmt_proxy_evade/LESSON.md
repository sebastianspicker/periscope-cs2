# vmt_proxy_evade — VMT Proxy Dispatch Evasion

Family: Evasion. Tiers: T2.

## Battlefield
Red: Intercepts game functions without modifying any vtable.
     Uses a proxy object that wraps the real interface.
Blue: Collects all 112 interface VMTs + entity VMTs, computes CRC,
     checks VMT offset from module base. Detects modified vtables.

## Real AC Context
CS2's CollectInterfacesData (Message 160) collects:
  - All 112 global interface VMT pointers
  - VMT offset from module base (CRC of offsets)
  - Entity VMT usage count (VMTs with count=1 are anomalous)
  
If red modifies a vtable pointer, the offset-from-base changes,
triggering CRC mismatch. VMTs appearing only once are flagged.

## Technique
Red creates a proxy object that:
  1. Has the same vtable layout as the real interface
  2. Forwards intercepted calls to the original
  3. Does NOT modify the original vtable pointer
  4. Is used only by red -> has usage_count=1
  
Blue detects if the proxy VMT is not from a known module base.
