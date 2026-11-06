# 168_capture_exclusion_diff - Capture exclusion vs frame diff

Family: Evasion. Tiers: T0. Area: t0. Sim only.

## Battlefield

Red: Mark the overlay window `WDA_EXCLUDEFROMCAPTURE`. The player sees the
outlines; a capture-based sensor sees only the clean game.

Blue: Capture the screen and diff it against the presented frame. Where an
overlay should be, the diff is empty - that is the scar.

Arena: sim::World only.

## RED

1. Spawn a presenter.
2. Present an overlay over the game window.
3. Set `wda_excluded_from_capture` (and the update counter), clear
   `capture_sees_overlays`, and set the diff to zero.

Achieved when: the overlay is excluded and the capture path sees nothing.

## BLUE

1. Arm the capture-diff sensor.
2. Note the capture path does not see overlay pixels.
3. Note the diff is empty while the presenter runs.
4. Note the exclusion flag itself.
5. Mitigation: require an out-of-band heartbeat (second device or signed
   window list) so the excluded overlay is not the only evidence.

Win: `detected := signals >= 2 && !capture_sees && diff == 0 && excluded`,
`mitigated := detected && risk >= 0.65`.

## Takeaway

Capture exclusion is an intentional blind spot, not a failure. The counter is
an independent observation path: if the player can see it and the capture
cannot, the difference is the signal.
