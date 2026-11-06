# 107_polymorphic_build - Polymorphic Builds

Family: Evasion. Scope: `sim::World` only.

## Context

Polymorphic distribution changes code layout, dead-code selection, and import ordering between builds. That can defeat exact file hashes, including normalized-hash pipelines that retain build-sensitive structure. It does not inherently change the tool's runtime purpose or its distribution lineage.

## Lab

Red records a unique build identifier and a synthetic module layout marker. It does not compile, mutate, pack, or execute another binary.

Blue treats the changing build ID as one signal and correlates it with a layout-family marker. The lesson is to retain provenance and behavioral features so a new hash does not become a clean slate.

## Takeaway

Hashing remains useful for clustering known samples, but durable detection needs behavioral and lineage correlation across build variants.
