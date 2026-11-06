# 165_handle_origin_chain - Duplicate chain vs owner-only attribution

Family: Evasion. Tiers: T0. Area: t0. Sim only.

## Battlefield

Red: Duplicate a game handle twice - donor to relay to consumer - so the
consumer's handle names the relay as owner, one hop short of the donor.

Blue: Walk the duplicate chain (owner of the owner's source handle) to the
root before scoring, instead of trusting the immediate owner.

Arena: sim::World only. The handle table carries acquisition models, so the
duplicate edges are visible even when the owner is not the origin.

## RED

1. Spawn donor, relay and consumer actors.
2. Donor opens VM_READ on the game (a normal handle edge).
3. Relay records a `HandleDuplicate` handle acquired from the donor
   (`via_proxy`).
4. Consumer records a second `HandleDuplicate` handle acquired from the relay.
5. Consumer reads the entity count through its duplicated handle.

Achieved when: the consumer's read succeeds with `handle_chain_depth == 2`.

## BLUE

1. Count `HandleDuplicate` entries targeting the game.
2. For each duplicate handle, look at the owner's own open handles. If the
   owner also holds a handle to the game, the origin is upstream - this is the
   misattribution an owner-only sensor reports.
3. Read the recorded chain depth.
4. Corroborate with read telemetry.
5. Mitigation: resolve to the root and require root reputation (signed or
   allow-listed) before ranked access.

Win: `detected := signals >= 2 && misattributed && duplicate_handles > 0`,
`mitigated := detected && risk >= 0.65`.

## Takeaway

Handle ownership is a snapshot, not a provenance record. A relay hop costs red
almost nothing and defeats "the owner is the cheat" heuristics. Blue needs the
duplicate graph, and the mitigation must land on the root process.
