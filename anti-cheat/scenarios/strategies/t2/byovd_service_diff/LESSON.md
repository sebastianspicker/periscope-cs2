# 167_byovd_service_diff - BYOVD service cleanup vs module-hash orphan

Family: Detection. Tiers: T2. Area: t2. Sim only.

## Battlefield

Red: Load a signed vulnerable driver, read through its device, then delete the
SCM service and registry key so the service database looks clean.

Blue: Hash the loaded-module list and diff the service database. A loaded hash
with no service entry is an orphan - cleanup removed the paperwork, not the
image.

Arena: sim::World only.

## RED

1. Spawn a reader.
2. Load a known-bad signed driver and create its mem-rw device.
3. Read the entity count through the IOCTL device.
4. Set `byovd_service_cleaned` and bump `service_db_diff_count`.
5. Set `loaded_module_hash_orphan` - the image stays resident.

Achieved when: the read works, the service is "cleaned", and the module hash
is still resident.

## BLUE

1. Check the module inventory for the image hash.
2. Match the hash against the vulnerable-driver catalog.
3. Diff the service database and note the removed entry.
4. Flag the orphan pair (service cleaned + module present).
5. Corroborate with driver-load telemetry.
6. Mitigation: the module inventory is authoritative; deny ranked access while
   the orphan is resident.

Win: `detected := signals >= 2 && module_present && orphan && diff > 0`,
`mitigated := detected && risk >= 0.70`.

## Takeaway

`sc delete` and a registry wipe clean the *service database*, not
`PsLoadedModuleList`. Cleanup that stops there is detectable until reboot.
Defenders should hash modules, not trust the service list.
