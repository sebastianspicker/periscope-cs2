# Overlay backend compatibility matrix (ledger V5)

Which presentation backend works where, and the known per-compositor quirks.
Sources: deadlocked's `compatibility.md` (the most complete WM/quirk matrix
found in the 2026-11-05 research pass), the house audit history (A7, A12–A14,
B17–B22), and this repo's backend implementations.

Status rows are claims from code reading and the upstream matrix — each row
carries a dated testing note; update the note when you verify a row on real
hardware. Nothing here was verified on Windows/macOS compositors at research
time (house style: runtime-unverified is stated, not implied).

## Backends in this repository

| Backend | Platform | Presentation | Status |
|---|---|---|---|
| `overlay_backends/win32.py` | Windows | `UpdateLayeredWindow` layered topmost window | Supported (A7/A12/A13/B17 fixed 2026-11-05) |
| `overlay_backends/cocoa.py` | macOS | `NSImageView` over raw RGBA via `NSBitmapImageRep` | Supported (A14 fixed 2026-11-05) |
| `overlay_backends/tkinter_backend.py` | cross | Tk `-transparentcolor` | Fair — off-Windows `-transparentcolor` fails and leaves a 0.95-alpha black window (audit note, unfixed by decision) |

## Windows notes

- Fullscreen-windowed game mode is required; exclusive fullscreen composites
  wrong (matches the whole upstream ecosystem's experience).
- Multi-monitor: the presenter stores x/y (A13 fix); a window spanning
  monitors pauses capture rather than compositing garbage.
- Hotkeys are registered **with a modifier** (Ctrl+Alt) since the A12 fix;
  bare-key registration hijacks the user's typing.
- Hide-from-capture (`SetWindowDisplayAffinity`) is the accessibility
  presentation feature for study recordings; it lives in the
  capture-presenter area excluded from ordinary review — extend it only with
  the same review posture, and never tune it against anti-cheat behavior
  (see root `SECURITY.md`).

## macOS notes

- The Cocoa backend shows frames from raw RGBA bytes (A14 fix: build
  `NSBitmapImageRep`, not `NSImage.initWithData_`).
- Screen-recording permission is required for capture (macOS TCC); without
  it the pipeline reports blank frames — surface that in the GUI, not just
  the log.

## Compositor quirks (from the upstream matrix, for future Linux work)

| Environment | Quirk class |
|---|---|
| GNOME (Mutter), KDE (KWin) | Best overlay support upstream; compositor must be enabled for transparency (KDE setting) |
| SwayWM, Weston | Good support upstream |
| i3, OpenBox, XFCE | Fair; click-through/always-on-top may need manual configuration |
| Hyprland | Poor X11 support for overlay techniques; XWayland quirks (upstream "no guarantees") |
| Gamescope | The game reports 16:9 internally, so window-resolution-derived overlay geometry is wrong — run without Gamescope |

## Click-through, always-on-top, transparency: the three failure modes

Every upstream overlay implementation eventually hits the same three
limitations. When a user reports "the overlay shows but I can't click" or
"the overlay is black", it is almost always the compositor, not this code —
the matrix above exists so support answers with the row, not a debugging
session.

## Maintenance rule

Add a row when a backend gains a platform, and date every status change.
A compatibility matrix without dates is a rumor, not a matrix.
