# Blue project structures (defense — what we build)

Blue is the anti-cheat / game-integrity engineering side. Structure below is a **suggested monorepo shape** for research → production detectors aligned to red T0–T3.

## Index

| Doc | Purpose |
|-----|---------|
| [`project-tree.md`](project-tree.md) | Full blue repo / component tree |
| [`control-stack.md`](control-stack.md) | Controls by priority and tier |

## Design principles

1. **Assume degradation** — red falls back T3→T2→T1; blue runs all layers.
2. **Prefer server truth** — fog-of-war and demos beat client cat-and-mouse long-term.
3. **Policy is a control** — VBS/HVCI/Secure Boot requirements are first-class for T3.
4. **No single signature** — polymorphic red; use graph, driver, trust, behavior.
5. **Delayed confidence** — ban on correlated signals + demo review, not one heuristic.

## Blue vs red ownership

```text
RED backend          BLUE owner component
─────────────────────────────────────────
RPM / syscall     →  handle_graph + process_watch
kernel ioctl      →  driver_guard
BYOVD             →  driver_guard.blocklist
HV + bridge       →  trust_policy + hv_probe + driver_guard
UI / phone        →  client_telemetry (weak alone)
offsets C2        →  threat_intel (optional)
entity data       →  netcode interest_mgmt
legit play look   →  info_advantage scoring
```
