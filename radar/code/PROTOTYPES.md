# Per-tier red / blue prototypes

Runnable lab prototypes.

## Executables

| Tier | Red | Blue | CS2 radar |
|------|-----|------|-----------|
| T0 | `proto_t0_red` | `proto_t0_blue` | `cs2_radar_t0` |
| T1 | `proto_t1_red` | `proto_t1_blue` | `cs2_radar_t1` |
| T2 | `proto_t2_red` | `proto_t2_blue` | `cs2_radar_t2` |
| T3 | `proto_t3_red` | `proto_t3_blue` | `cs2_radar_t3` |
| T4 | `proto_t4_red` | `proto_t4_blue` | `cs2_radar_t4` |

## Sources

```text
demos/proto_t0_red.cpp
demos/proto_t0_blue.cpp
demos/proto_t1_red.cpp
demos/proto_t1_blue.cpp
demos/proto_t2_red.cpp
demos/proto_t2_blue.cpp
demos/proto_t3_red.cpp
demos/proto_t3_blue.cpp
demos/proto_t4_red.cpp
demos/proto_t4_blue.cpp

# CS2 radar variants
demos/cs2_radar/tN/{cs2_radar.cpp,main.cpp}
```

## What each pair demonstrates

### T0
- **Red:** attach lab fixture → parse entities → radar blips; exposes usermode handle flag.
- **Blue:** ingest VM_READ handle edge + radar-named co-run → risk/telemetry.

### T1
- **Red:** staged loader + XOR offset blob + syscall-shaped backend (still has handle).
- **Blue:** detect via handles while `hooks_saw_rpm=false` + staging RX/stub watch.

### T2
- **Red:** simulated BYOVD load + IOCTL backend (no game handle) → entities/blips.
- **Blue:** empty handle graph; catch via BYOVD blocklist, device open, callback integrity.

### T3
- **Red:** HV init denied when VBS on; HV+bridge when off; fallback chain T3→T2→T0.
- **Blue:** ranked TrustPolicy deny; HV probe anomaly; bridge device; attestation; ban correlator + info-advantage residual.

### T4
- **Red:** DMA device present + IOMMU off → entity read without local cheat process.
- **Blue:** weak platform IOMMU signal; structural fog + info-advantage residual.

## Build & run

```bash
cmake -S . -B build -DLR_BUILD_PROTOS=ON -DLR_BUILD_TESTS=ON
cmake --build build
./build/proto_t0_red
./build/proto_t0_blue
# ...
ctest --test-dir build --output-on-failure
```

## Pairing workflow

1. Run `proto_tN_red` — note which surface flags it prints (`exposes_usermode_handle`, device name, …).
2. Run `proto_tN_blue` — confirm detection path fires.
3. Extend blue modules until red’s printed surface is covered.
