# Build and Test

## Configure and Build

From the repository root:

```
cmake -S radar -B radar/build/dev
cmake --build radar/build/dev -j
```

The radar tree needs the sibling anti-cheat checkout (default `../anti-cheat`,
override with `-DLR_ANTI_CHEAT_DIR=…`): radar consumes the lab's types,
simulation world and strategy libraries through that one-way seam, so a radar
configure always configures the lab's library targets too. Lab-only flags set
on the radar configure line (`LR_BUILD_STRATEGY_LAB`, `LR_BUILD_PROTOS`,
`LR_BUILD_DUELS`, `LAB_ALLOW_*`) are defined in
[`../../anti-cheat/cmake/Options.cmake`](../../anti-cheat/cmake/Options.cmake)
and flow into this configure; the verify lane below passes
`-DLR_BUILD_STRATEGY_LAB=ON` so the catalog runner is available in radar's
build directory.

Real backends (Windows):

```
cmake -S radar -B radar/build/dev -DLR_ENABLE_REAL_ALL=ON
cmake --build radar/build/dev -j
```

## Options

Options defined in this tree (`radar/cmake/Options.cmake`):

| Option | Default | Description |
|--------|:-------:|-------------|
| `LR_ENABLE_REAL_RPM` | OFF | Real Windows ReadProcessMemory backend |
| `LR_ENABLE_REAL_SYSCALL` | OFF | Real direct syscall backend |
| `LR_ENABLE_REAL_KERNEL` | OFF | Real kernel/BYOVD backend |
| `LR_ENABLE_REAL_VMX` | OFF | Real Intel VT-x hypervisor backend |
| `LR_ENABLE_REAL_DMA` | OFF | Real DMA/PCIe backend |
| `LR_ENABLE_REAL_SMM` | OFF | Real SMM/firmware backend |
| `LR_ENABLE_REAL_GPU` | OFF | Real GPU rendering/compute backends |
| `LR_ENABLE_REAL_NET` | OFF | Real networking/C2 backends |
| `LR_ENABLE_REAL_UEFI` | OFF | UEFI firmware analysis backend |
| `LR_ENABLE_REAL_LINUX` | OFF | Linux live backend and portable host stubs |
| `LR_ENABLE_REAL_ALL` | OFF | Enable all real mode backends (Windows only) |
| `LR_ENABLE_VEH` | OFF | VEH anti-debug handler (kernel-visible) |
| `LR_ENABLE_SHELLCODE_INJECTION` | OFF | Isolated Windows x64 shellcode research adapter (requires `LR_ENABLE_REAL_SYSCALL=ON`) |

All real and live-reader options default to `OFF` on every platform, and real
backend features are unsupported on Apple. Lab/simulation options and the
simulation seed generation live in
[`../../anti-cheat/cmake/Options.cmake`](../../anti-cheat/cmake/Options.cmake)
(`LR_BUILD_STRATEGY_LAB`, `LR_BUILD_PROTOS`, `LR_BUILD_DUELS`, the
`LAB_ALLOW_*` simulation capability flags and the `LR_BUILD_ID` watermark
identifier); see the lab's build docs for their full list.

## Directory layout

| Kind | Path (from the Radar root) |
|------|------|
| Radar libraries | `src/domain/radar/`, `src/application/radar/` |
| Real backends | `adapters/real/` |
| Demos | `apps/demos/` (flat `.cpp` + `apps/demos/cs2_radar/tN/`) |
| Examples | `examples/drivers/`, `examples/firmware/` |
| Relay | `relay/` (Python, own README) |
| Host tests | `tests/radar_*_test.cpp` |
| Project docs | `docs/` (product index, architecture, build, DMA policy) |

The lab's libraries, team components and strategy pairs moved to
[`../../anti-cheat/`](../../anti-cheat/README.md); radar only consumes them
through `LR_ANTI_CHEAT_DIR`.

## Test

```bash
ctest --test-dir radar/build/dev --output-on-failure   # portable host tests
./radar/build/dev/strategy_lab all --quiet             # strategy pairs (lab catalog)
```

`ctest` covers the nine `radar_*` host tests: snapshot, sampling, ring,
geometry, offsets, scatter, LOS, webradar and tri_loader. They link only the
pure radar libraries (`radar_snapshot`, `radar_webradar`, `radar_collision`),
so the suite runs on every platform with the default configuration.

The radar-sim verify lane from the repository root does all of it in one go:

```bash
python3 scripts/verify.py radar-sim
```

It configures and builds `radar/` with `-DLR_BUILD_STRATEGY_LAB=ON`, expects
CTest to report 9/9, then runs the anti-cheat lab's strategy catalog from
radar's build directory (`strategy_lab list`, `strategy_lab all --quiet`,
187 pairs, 0 failures). The catalog itself and the lab-only verification lane
(`verify.py anti-cheat-sim`, which owns the `stack_posture_test` now that the
posture test moved with the lab) are documented in
[`../../anti-cheat/CONTRIBUTING.md`](../../anti-cheat/CONTRIBUTING.md) and
[`../../anti-cheat/README.md`](../../anti-cheat/README.md).

A clean checkout includes the suite used by `ctest`. Other local test and lab
directories remain ignored.

## Common Binaries

| Binary | Source | Description |
|--------|--------|-------------|
| `radar_t0` … `radar_t4` | `apps/demos/radar_t{0-4}.cpp` | Per-tier deterministic simulation demos on the lab's `sim::World` |
| `live_radar` | `apps/demos/live_radar.cpp` | Opt-in live CS2 radar overlay (real RPM + syscall + GPU) |
| `cs2_radar_t0` … `cs2_radar_t4` | `apps/demos/cs2_radar/t{0-4}/` | Alternative CS2 radars (real `t0`, sim fallback) |
| `tier_comparison` | `apps/demos/tier_comparison.cpp` | All-tier timing comparison |
| `full_prototype` | `apps/demos/full_prototype.cpp` | Opt-in real-backend exercise |
| `strategy_lab` | `../../anti-cheat/scenarios/strategies/framework/main.cpp` | Strategy catalog runner, built into this tree's build dir |

`live_radar` takes `--full` (pin all-entity intel) and `--strict-intel` (pin
hud-echo, fail closed). Policy, shaping, and the degrade ladder:
[STEALTH-POSTURE.md](../../anti-cheat/docs/STEALTH-POSTURE.md).

Targets backed by `adapters/real/` are created only when their required
capability targets exist. The default target graph contains no real adapter
source.
