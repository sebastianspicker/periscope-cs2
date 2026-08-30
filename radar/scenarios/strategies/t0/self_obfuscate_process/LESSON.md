# self_obfuscate_process — Process Self-Obfuscation

Family: Evasion. Tiers: T0-T1.

## Battlefield
Red: Disguises the radar process to blend in with legitimate tools.
     Mimics: process name, window class, PEB data, working set, 
     module list, command line, and timing patterns.
Blue: Enumerates processes looking for anomalies: unexpected names,
     window properties, working set patterns, parent lineage.

## Real AC Context
VAC's VAS walk + module enumeration baselines ALL processes.
Tools that stand out (unusual name, size, window, timing) get
closer inspection. Disguising as RTSS/Afterburner/Discord/Steam
overlay is a common T0 evasion.

## Techniques
- Process name: "rtss.exe", "discordoverlay.exe", "obs64.exe"
- Window: matches class name of disguised tool
- PEB: set Appropriate flag, match subsystem
- Working set: trim to expected size
- Timing: jittered read pattern, frame pacing like GPU monitoring
- Command line: matches expected args of disguised tool
