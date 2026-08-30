# module_list_hide — Module List Snapshot Evasion

Family: Evasion. Tiers: T2.

## Battlefield
Red: Hides a loaded module from CDllVerificationMonitor's module list snapshot.
Blue: CS2 captures a full module list via CModuleListSnapshot::Capture(). Any
      module visible in the process but missing from the snapshot is flagged.

## Real AC Context
CS2's CModuleListSnapshot:
1. Calls EnumProcessModulesEx to get all modules
2. For each module, captures base, size, path, timestamp
3. Cross-references with BSecureAllowed trust check
4. Reports any module not in expected list (Message 159)

Red must either:
- Remove the module FROM the enumeration (DKOM-style unlinking)
- Hook the enumeration API to exclude the module
- Manual map (no module entry at all — but then other detections apply)

## Lesson
Simple module enumeration catches injected DLLs. Red must unhook from
the module list or use manual mapping with additional evasion.
