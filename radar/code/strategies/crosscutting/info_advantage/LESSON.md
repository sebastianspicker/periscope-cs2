# 25_info_advantage — Info-advantage behavior

Family: Structural. Tiers: all. Area: xc/structural. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Play with unfair knowledge, human aim

Blue: Score pre-aim/track without vision or sound

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::info_advantage::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Legit radar play — Multi-frame pre-aim without vision/sound; optional radar SaaS net.
2. Step 1: multi-frame hostile demo — pre-aim without vision/sound.
3. Step 2 (optional residual): radar SaaS net for correlation path.

Team / depth APIs used:
- `server::DemoFrame`

World scars and lab surfaces (from shipped red code):
- add_net() — Plant NetFlow residual (radar SaaS / C2).

Achieved when: `r.frames_planted`

## BLUE

Entry: `examples::info_advantage::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Classic + multi-invariant scorers — InfoAdvantageScorer + MultiInvariantScorer; residual correlation.
2. Path A: classic InfoAdvantageScorer on demo frames.
3. Path B: multi-invariant depth scorer (vision + latency + reports).
4. Path C: world residual (radar SaaS planted by red).

Team / depth sensors:
- `server::DemoFrame`
- `server::InfoAdvantageScorer`
  - on_frame DemoFrame: aim on hidden target without vision/audio accumulates hits/score.
- `depth::MultiInvariantScorer`
  - RichDemoFrame path: vision_hits, latency, peer reports, delayed action / overwatch threshold.
- `depth::RichDemoFrame`
- `depth::DelayedAction`

Multi-reason / result fields and sensors:
- result field `hits`
- result field `score`
- result field `classic_hit`
- result field `multi_hit`
- result field `residual_hit`
- result field `multi_reason`
- result field `detected`
- local `reasons` init=0

Win conditions for this pair:
- detected := `r.classic_hit || r.multi_hit || (r.residual_hit && cr.hits >= 2)`
- Pass narrative: detection-oriented — mitigated stays false; multi-reason detect is the win.

## Takeaway

Information advantage is structural: who sees whom on the wire. Interest management and encrypted streams shrink red even when client AC is blind.

## Run

```bash
./build/strategy_lab run 25_info_advantage
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `info_advantage/red_example.cpp` — full red multi-step
- `info_advantage/blue_example.cpp` — full blue multi-reason
- `info_advantage/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/multi_invariant_scorer.hpp`
