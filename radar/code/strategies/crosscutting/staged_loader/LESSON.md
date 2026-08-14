# 111_staged_loader - Staged Loading

Family: Delivery. Scope: `sim::World` only.

## Context

Staged delivery can separate a small bootstrapper from a later encrypted component so no one file contains all functionality. Defenders should correlate network provenance, staging artifacts, and subsequent execution behavior rather than treating a download alone as malicious.

## Lab

Red records a synthetic flow to reserved `lab.invalid`, an encrypted stage-two section, and a memory-only transition flag. It does not connect to a network, download data, decrypt a payload, inject into a process, or execute code.

Blue requires the flow, staging section, and transition artifact before simulated mitigation.

## Takeaway

Time-correlated telemetry across network, process, memory, and fileless execution surfaces is more resilient than scanning a first-stage file in isolation.
