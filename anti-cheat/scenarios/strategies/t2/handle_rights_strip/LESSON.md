# 154_handle_rights_strip - Handle-rights strip vs kernel-attach read

Family: Delivery. Tiers: T2. Area: t2. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Read game memory through a kernel attach (KeStackAttachProcess-style context switch) that never opens a handle, so handle-right hardening has nothing to intercept

Blue: Arm an ObRegisterCallbacks-style pre-operation strip on the protected pid and detect the attach-read scar with independent reasons

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::handle_rights_strip::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Validate the game process and its entity table (arena plants lab entities).
2. Spawn a `handle-strip-actor.exe` lab actor.
3. Verify a clean slate: the actor holds no handle edge at all.
4. Establish the attach context: set `World.kernel_attach_read_active` (the lab model of KeStackAttachProcess + direct field reads).
5. Read the entity count through the attach path with `read_mem(..., require_handle=false)` and bump `kernel_attach_read_ops`.
6. Verify the scar: reads succeeded while the handle graph still has zero foreign VM_READ edges.

Real-world mechanism mapped to lab scars:

- ObRegisterCallbacks / ObGetObjectCallbacks strips handle rights at handle
  creation. It only ever sees handle traffic - so red models the bypass by
  never creating a handle. `kernel_attach_read_active` +
  `kernel_attach_read_ops` are the attach scars; `handles_to(game)` staying
  empty is the bypass evidence.

Achieved when: attach reads decoded entity data with zero VM_READ handles in the graph.

## BLUE

Entry: `examples::handle_rights_strip::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Reason 1 (strategy scar): `kernel_attach_read_active` residual is set.
2. Reason 2: `kernel_attach_read_ops > 0` - attach read telemetry exists.
3. Reason 3: reads occurred with no foreign VM_READ handle in the graph - the attach bypass signature (a plain RPM reader would have left an edge).
4. Reason 4: an untrusted mem-rw driver is present (attach needs a kernel foothold).
5. Mitigation: arm `ob_handle_rights_strip_active` on the protected pid (`rights_strip_target_pid`) and count `rights_strip_downgrades` for every VM_READ/VmWrite edge that a future open would lose.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar`
- mitigated := `detected && strip armed && risk >= 0.65` (ranked deny follows)
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

Handle-rights hardening (ObRegisterCallbacks) is a handle-surface control. A
reader that attaches to the process context instead of opening a handle is
invisible to it - blue must watch attach/context-switch-class residuals and
pair the strip with kernel telemetry, not rely on the handle graph alone.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 154_handle_rights_strip
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T2
```

Open the pair sources beside this lesson:

- `handle_rights_strip/red_example.cpp` - full red multi-step
- `handle_rights_strip/blue_example.cpp` - full blue multi-reason
- `handle_rights_strip/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
