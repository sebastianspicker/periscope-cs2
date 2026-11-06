# Decision: keep the four products independent

Status: accepted; dependency shape amended 2026-11-06 (the anti-cheat split
added the one allowed build-time edge)

The repository holds four separate projects: Anti-Cheat Lab, Radar, Vision
and Lens. They have different users, runtimes, dependencies, threat models,
and release units, and each keeps its own license (`anti-cheat/` and
`radar/` MIT; `vision/` and `lens/` AGPL-3.0-only).

The projects stay independent at runtime and in their evidence:

- Vision and Lens never import Radar or the lab; Radar and the lab never
  import Vision or Lens.
- The sole build-time exception is the one-way radar → anti-cheat edge:
  Radar consumes the lab's simulation libraries through
  `LR_ANTI_CHEAT_DIR`. The lab never imports Radar CMake or sources (the
  test-only `LR_RADAR_DIR` posture header is the documented exception).
- Similar-looking code is duplicated across a license or runtime boundary
  unless it represents a deliberately licensed external package.

The repository root may provide documentation, CI routing, and verification
dispatch only. It must not contain shared runtime source.
