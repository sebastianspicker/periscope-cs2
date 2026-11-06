# Map collision data

`live_radar` looks for `<map-name>.tri` here and loads it in the background.
The format is documented in `../../src/application/radar/tri_loader.hpp`
(`TRIZ`, version 1, triangle soup). With no file the radar simply omits the
geometry LOS hint; the memory `m_bSpotted` path is unaffected.

Two ways to produce a file:

1. **From pre-baked donor `.opt` maps** (fast, no game assets):

   ```bash
   python radar/scripts/opt_to_tri.py --input-dir <donor>/maps --output-dir radar/data/maps
   ```

2. **From the game's `.vphys` files** (PhysExtractor + VPhysToOpt, then the
   converter above). The extraction tools are Windows/.NET; the converter is
   portable and strict.

`LR_CS2_MAP_DIR` overrides the directory at runtime.

Caveats:

- Static geometry only. Smoke, molotovs and dynamic props are not represented.
- A `.tri` file is map-name keyed, so a workshop map needs its exact
  sanitised name (spaces become `_`).
- LOS is a hint, not ground truth. Keep `los_visibility` off if a map file is
  known to be incomplete.
