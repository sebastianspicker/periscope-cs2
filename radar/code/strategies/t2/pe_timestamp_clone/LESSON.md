# pe_timestamp_clone — PE Timestamp Clone Evasion

Family: Evasion. Tiers: T2.

## Battlefield
Red: Clones a legitimate module's PE timestamp (e.g., client.dll 0x66800000)
     onto a manual-mapped cheat module so a naive PE timestamp verification
     sees an allowlisted value.
Blue: Runs PE timestamp verification for known modules, then cross-checks
     module identity (PEB linkage / text hash) so a cloned timestamp cannot
     hide a foreign module.

## Real AC Context
CS2's diagnostic system (CDllVerificationMonitor) collects module snapshots
that include each loaded module's PE timestamp. A manual-mapped module that
is not linked into the PEB is already suspicious; copying a known-good
timestamp onto it defeats checks that only compare the timestamp field,
but not the module's identity or origin.

## Technique
1. Red maps the cheat module without PEB linkage (manual map)
2. Red records the foreign (non-cloned) PE timestamp baseline
3. Red copies the legitimate client.dll timestamp onto the cheat module
4. Red runs the simulated PETimestampSensor against the clone
5. Blue verifies timestamps AND module identity: a foreign module carrying
   a legitimate timestamp is still flagged

## Lesson
Timestamp cloning beats timestamp-only verification. Blue must combine
timestamp checks with module identity (PEB linkage, text hash, origin) so a
borrowed timestamp is not enough to appear legitimate.

## Dual mode
SIM: Uses sim::World diagnostic module list and cs2::sensors::PETimestampSensor.
REAL: Would patch the PE header timestamp of the in-memory module before the
      diagnostic snapshot is collected.
