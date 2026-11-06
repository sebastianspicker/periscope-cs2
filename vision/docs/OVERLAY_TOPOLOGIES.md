# Overlay topologies — a comparison

How composited outlines can reach the player's eye. The vision track's
primary deployment is the second-device composite
([CONTOUR_PIPELINE.md](CONTOUR_PIPELINE.md)); this page places that choice in
the design space, with the failure modes each topology must survive. The
assessment ledger (E30) sourced the real-world examples.

## The four topologies

| | In-process present hook | Host-window adoption | External layered window | Second-device composite (this project) |
|---|---|---|---|---|
| Where the drawing runs | Inside the game process, in its `Present` call | Another process's existing topmost window (e.g. a chat overlay) | Own process, own layered window (`WS_EX_LAYERED` + `UpdateLayeredWindow`) | Second Windows machine fed by the HDMI splitter |
| Game machine footprint | Highest: injected module, hooked swapchain, engine-primitive hooks | A window the game machine already had | One extra process + window | Zero (a capture card is indistinguishable from a monitor) |
| Latency | Lowest (composited into the game's frame) | Low | Low-medium (one compositor pass) | Highest (capture → infer → composite → present); bounded by `--max-capture-age-ms` |
| Ownership of the surface | Game's swapchain | Another application's — fragile against its updates | Ours | Ours (device 2) |
| Input interplay | Menu needs WndProc hook + focus juggling | Must poll keys when not owning the WndProc | Own WndProc, click-through via `WS_EX_TRANSPARENT` | None on the game machine |
| Resize/display change | Must recreate the RTV on `ResizeBuffers` | Follows the host window | Own `WM_SIZE` handling | Capture fourcc/resolution renegotiation |
| Anti-cheat-visible surface | Everything | Reduced (no new window) | One window | Nothing on the game machine |

## Why the second-device path is the default here

The vision track never needs the game machine to run anything: the HDMI
split happens before anything that runs code, so the game machine cannot
distinguish the capture card from a monitor. That is the whole argument of
[CONTOUR_PIPELINE.md](CONTOUR_PIPELINE.md); the cost is the latency budget,
which is why the presenter is 1:1 native and late frames are dropped rather
than queued.

The other three topologies are documented for completeness and for the
robustness lessons they carry — they are implemented elsewhere (the radar
track owns the D3D11 in-process overlay for its own display), not by the
vision package.

## Failure modes every topology must survive

Real implementations of the first three topologies (assessment sources
E30/VEL, E30/EXT) demonstrate the canonical robustness cases; the vision
presenter carries the analogous obligations:

1. **Resize** — in-process hooks must recreate the render-target view when
   the swapchain calls `ResizeBuffers`; a presenter must handle `WM_SIZE`
   (or, for the SHM ring, a resolution change of the published frames: the
   presenter rejects a size mismatch loudly rather than scaling).
2. **Device/surface loss** — a dropped D3D device (or a disconnected capture
   card) must degrade to "no overlay" with a clear diagnostic, never to
   garbage pixels.
3. **Surface ownership** — adopting another process's window breaks when
   that process updates its window tree; owning the surface (or owning
   nothing at all, on device 2) avoids the class entirely.
4. **Input focus** — click-through (`WS_EX_TRANSPARENT`) vs interactive menu
   is a mode, not a property; flipping it mid-session must not steal focus
   from the game.
5. **Stale frames** — a presenter fed by a ring must detect torn/late frames
   (the SHM contract's frame-index re-check) and drop rather than stutter.

## Cross-references

- The second-device pipeline contract: [CONTOUR_PIPELINE.md](CONTOUR_PIPELINE.md)
- The presenter's capture-exclusion fallback ladder: `presenter.py`
  (`streamproof_mode`)
- The SHM wire contract: `adapters/capture/sinks/shared.py`
