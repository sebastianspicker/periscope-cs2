# 108_anti_re_canary - Anti-RE Canary

Family: Evasion. Scope: `sim::World` only.

## Context

Anti-analysis code may place integrity canaries around sensitive state and alter behavior when it believes a debugger, virtual machine, or sandbox is present. Such checks are unreliable and often leave stronger telemetry than the analysis environment they try to identify.

## Lab

Red uses only the simulator's `analysis_host` flag to decide whether a canary tripped. It then records two modeled debugger-state anomalies. No debugger is queried, no process is terminated, and no memory is erased.

Blue looks for disagreement between independent debugger-state views and canary behavior. Multiple inconsistent signals are required before the simulated ranked policy is denied.

## Takeaway

Detection should validate state from independent sources and treat abrupt anti-analysis state changes as a signal, while allowing for legitimate developer and accessibility tools.
