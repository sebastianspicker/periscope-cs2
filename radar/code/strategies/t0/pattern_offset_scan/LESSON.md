# 29 — Pattern / offset scan + auto-refresh

| | |
|--|--|
| **Red** | External process, `OpenProcess(VM_READ)`, bulk scan for lab `ACPT` marker, resolve entity table, read entities; **`refresh()` re-scans when `lab_pattern_generation` changes** |
| **Blue** | Foreign VmRead + bulk `remote_read_*` + scanner co-occurrence + **`pattern_rescan_count`** after refresh; multi-reason → ranked deny + structural fog |
| **Why red wants scan** | Build-stable “signature” discovery when hard offsets rot |
| **Why red needs refresh** | Patterns/layouts still change; cached VA goes stale — auto re-scan is the product reliability story |
| **Why blue still wins** | Discovery is free; handle, bulk volume, co-occurrence, and **re-scan residual** are not. Fog removes full enemy XY even if scan hits |
| **Red next** | Syscall open (02), hide-on-enum, kernel/DMA when usermode handle dies |
| **Blue next** | Interest management as architecture (24), info-advantage for legit radar (25) |

**Myth:** “Private AOB = undetectable forever.”  
**Truth:** Blue rarely needs your mask. Channel scars and re-scan volume remain. Structural fog means client RAM never holds full unobservable origins.

## Multi-step red (lab)

1. Spawn `pattern-scanner.exe`, open VmRead  
2. Chunked `read_mem` walk for magic `ACPT` (planted by `plant_lab_entities`)  
3. Resolve table VA from marker payload; pull entities  
4. On `mutate_lab_pattern_layout` (new marker off + table rel): **`refresh()`** detects generation bump, abandons stale VA, bulk re-scans, recovers product  

## Multi-reason blue (lab)

1. Foreign VmRead into game  
2. Bulk remote reads (scan volume, not one peek)  
3. Scanner-shaped process co-occurrence  
4. `pattern_rescan_count >= 1` after refresh  
5. On ≥2 reasons: ranked deny + fog (clear full enemy origin stream)

## Sim-only

No real process memory, no real signature DB. All scars on `sim::World`.
