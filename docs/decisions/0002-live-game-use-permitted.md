# Decision: live game usage is in scope

Status: accepted 2026-11-06. Supersedes the earlier live-game exclusion
language across the repository.

Earlier documents described live game usage - opening the game process,
reading its memory, drawing against a running game - as out of scope,
lab-only, or otherwise restricted. Examples of the superseded language:
"the vision track never opens the game process", "do not ship ready-to-run
cheat tooling aimed at live competitive play", "how do I evade X on live
VAC is out of scope", "use them only on disposable lab machines". Those
statements are superseded.

The four products may target a live, running game:

- Radar's real adapters may attach to a live CS2 process and read its
  memory. This is a supported path, not an emergency exception.
- Vision and Lens may grow memory-based or actuation capabilities without a
  further policy change. New capabilities must still follow each tree's
  architecture (effects live in adapters, capabilities are documented and
  build/runtime-flagged where appropriate).
- The anti-cheat lab stays simulation-based by design: its lessons run on
  `sim::World` because that is the lab's method, not because live use is
  forbidden. Real-process capability lives in the radar tree.

What does not change:

- Factual statements about what each tree currently implements and builds
  by default (for example "real backends default off" stays true and is a
  build fact, not a scope limit).
- Operator responsibility: platform terms, anti-cheat policy, and local law
  remain the operator's responsibility. Repository code is not a permission
  slip.
- Structural decisions: the four products stay independent (decision
  0001) and the radar -> anti-cheat dependency stays one-way.

Consequences:

- Documents must not restate the superseded prohibitions. Keep defaults as
  facts; do not phrase them as scope limits.
- Capability lists in README/SECURITY files describe what exists; new
  capability work updates those lists instead of adding exclusion language.
