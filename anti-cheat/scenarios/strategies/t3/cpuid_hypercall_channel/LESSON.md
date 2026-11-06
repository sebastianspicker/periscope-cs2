# 155_cpuid_hypercall_channel - CPUID-magic-key hypercall covert channel

Family: Delivery. Tiers: T3. Area: t3. Sim only - no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Probe CPUID leaves for a magic key exposed by the platform hypervisor, then read game memory through unauthenticated hypercalls - no driver, no game handle

Blue: Detect abnormal CPUID leaf probing and unauthenticated channel abuse, then require an authenticated channel

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::cpuid_hypercall_channel::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Validate the game process and its planted entity table.
2. Spawn a `cpuid-channel-actor.exe` consumer process.
3. Require the platform hypervisor posture (`Trust.platform_hv_active` + vendor, VBS off) - the pair seeds this precondition in `pair.cpp`, exactly like the sibling `hypercall_mem_read` pair.
4. Probe CPUID leaves for the magic key: `cpuid_leaf_probes = 3`, `cpuid_channel_magic = 'ACHV'`.
5. Open the covert channel: `cpuid_hypercall_channel_active = true` and, because the leaf answers any local process that knows the magic, `cpuid_channel_unauthenticated = true` with `cpuid_channel_consumer_pid` set to the actor.
6. Read the entity count through the channel (`hypercall_channel_reads++`) and verify the handle graph stays clean - no VM_READ edge is ever opened.

Real-world mechanism mapped to lab scars:

- Real CPUID hypercall channels (e.g. `vmcall`/`vmmcall` gates keyed by a
  magic leaf value) are invisible to handle and driver telemetry. The lab
  collapses this into `cpuid_hypercall_channel_active` +
  `hypercall_channel_reads`; `cpuid_leaf_probes` models the probing pattern
  blue can see in CPUID-usage telemetry; `cpuid_channel_unauthenticated`
  models the "any local process can use the channel" design flaw.

Achieved when: the channel served entity data with zero VM_READ handles.

## BLUE

Entry: `examples::cpuid_hypercall_channel::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Reason 1 (strategy scar): the covert channel is active with `hypercall_channel_reads > 0`.
2. Reason 2: abnormal CPUID leaf probing (`cpuid_leaf_probes >= 3`) - legitimate guests rarely scan vendor leaf ranges.
3. Reason 3: unauthenticated channel abuse - a foreign local process (neither game nor AC) consumed the channel.
4. Reason 4: no foreign VM_READ handle in the graph - handle telemetry is blind, which corroborates a covert path.
5. Reason 5: platform hypervisor posture that makes the interface reachable.
6. Mitigation: invalidate the unauthenticated magic (`cpuid_channel_unauthenticated = false`) so only vetted callers can re-arm the leaf; ranked deny at high risk.

Win conditions for this pair:

- detected := `signals >= 2 && specific_scar`
- mitigated := `detected && channel authentication enforced && risk >= 0.70`
- Pass narrative: blue_detected OR blue_mitigated (strategy_lab pass rule).

## Takeaway

A hypervisor covert channel removes every classic scar (handle, driver, DMA),
but the CPUID-probing pattern and the channel's own lack of authentication are
new residuals. Blue's answer is CPUID-usage telemetry plus channel
authentication - not more handle auditing.

## Run

```bash
./anti-cheat/build/dev/strategy_lab run 155_cpuid_hypercall_channel
```

Optional filters:

```bash
./anti-cheat/build/dev/strategy_lab list
./anti-cheat/build/dev/strategy_lab run --tier T3
```

Open the pair sources beside this lesson:

- `cpuid_hypercall_channel/red_example.cpp` - full red multi-step
- `cpuid_hypercall_channel/blue_example.cpp` - full blue multi-reason
- `cpuid_hypercall_channel/pair.cpp` - StrategyEntry wiring + narrator

Related shared headers pulled by this pair:

- `strategies/strategy_support.hpp` (risk scoring / driver inventory)
