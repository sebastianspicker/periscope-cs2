# 106_obfuscation_crypto - Cryptographic Obfuscation

Family: Evasion. Scope: `sim::World` only.

## Context

Cheat distributors often encrypt offset tables, configuration blobs, and inter-process messages to make simple string and byte signatures less useful. XOR with a fixed key is obfuscation, not strong encryption: analysts can recover it from the decrypt loop or known plaintext. Real products may use authenticated encryption and per-session keys, but crypto does not remove behavioral evidence.

## Lab

The red example performs a reversible XOR transform over a local teaching blob, then records encrypted-stream and key-presence telemetry. It never reads a game, contacts a service, or creates IPC outside the simulator.

The blue example correlates simultaneous encrypted-stream/key metadata with an encrypted offset-cache section. This models the principle that anti-cheat should combine runtime provenance and telemetry rather than rely on static signatures alone.

## Takeaway

Encryption can conceal content from static scanning, but weak keys, decrypt routines, key custody, and surrounding process behavior remain useful detection surfaces.
