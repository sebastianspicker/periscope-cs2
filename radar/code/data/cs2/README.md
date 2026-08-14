# CS2 offset / signature snapshots

Educational lab data for live demos (`live_radar`, `radar_t0`).

## Update (recommended)

From `code/`:

```bash
python scripts/update_cs2_signatures.py
# or via CMake:
cmake --build build --config Release --target update_cs2_signatures
```

This pulls the public [a2x/cs2-dumper](https://github.com/a2x/cs2-dumper) output and regenerates:

| File | Purpose |
|------|---------|
| `offsets.json` | Raw dumper globals (module RVAs) |
| `client_dll.json` | Raw schema field dump |
| `offsets_snapshot.json` | Trimmed snapshot demos can hot-load at runtime |
| `cs2_offsets_snapshot.hpp` | Mirror of the embedded header |
| `lib/real/cs2/offsets_snapshot.hpp` | Compiled-in defaults for demos |

## Runtime hot-load

`resolve_offsets_for_process` searches (first hit wins):

1. `LR_CS2_OFFSETS_PATH` env var
2. `./offsets_snapshot.json` (next to the demo exe)
3. `./data/cs2/offsets_snapshot.json`
4. relative parent paths used in-repo

Post-build copies place `offsets_snapshot.json` next to `live_radar` / `radar_t0`.

## After a game update

1. Run the updater
2. If values changed: rebuild demos (embedded header) **or** just re-copy the JSON next to the exe
3. Relaunch CS2 + the demo

## Check without writing

```bash
python scripts/update_cs2_signatures.py --check
# exit 0 = up to date, exit 2 = dump differs from local snapshot
```
