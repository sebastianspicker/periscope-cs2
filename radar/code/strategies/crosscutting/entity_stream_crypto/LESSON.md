# 40_entity_stream_crypto — Entity stream crypto

Family: Structural. Tiers: all. Area: xc/structural. Sim only — no real malware, VMX, BYOVD, DMA, or SMM.

## Battlefield

Red: Read/decrypt client entity stream when key is present

Blue: Encrypt without client key + interest management

Arena: sim::World only. Both sides share the same scar surface; neither side is a stub one-liner.

## RED

Entry: `examples::entity_stream_crypto::apply` in `red_example.cpp` / `red_example.hpp`.

What red does in the lab (read this side of the pair first):

1. Narrator move: Decrypt entity stream in-process — Encrypted stream + client key → exfil key; optional handle residual.
2. Step 1: product encrypts entity stream (ciphertext on wire / in buffer).
3. Step 2: session key still lives in the untrusted client process.
4. Step 3: multi-step key exfil via depth stream-exfil red path.
5. Step 4 (optional): delivery residual — mild foreign VmRead handle.

Team / depth APIs used:
- `depth::run_stream_exfil_red`

Expanded team path (what the wrapper actually does on sim::World):
- [depth::run_stream_exfil_red] Want full replication when server_sends_full_enemy_origin; attempt stream key exfil if encrypted.
- [depth::run_stream_exfil_red] useful_radar when entity stream still feeds a radar without client inject.

World scars and lab surfaces (from shipped red code):
- entity_stream_encrypted — World.entity_stream_encrypted = true
- client_has_stream_key — World.client_has_stream_key = true
- open_process() AccessMask::VmRead — OpenProcess-style handle scar (owner→target, access mask). AccessMask::VmRead
- spawn() — Spawn actor process on World process list.
- server_sends_full_enemy_origin / entity stream crypto / stream_key_exfiltrated surfaces.
- useful_radar when replication still feeds radar without inject.

Achieved when: `ex.key_exfiltrated && r.has_stream_key && r.stream_encrypted`

## BLUE

Entry: `examples::entity_stream_crypto::detect` in `blue_example.cpp` / `blue_example.hpp`.

What blue does in the lab:

1. Narrator counter: Server-side stream crypto — Encrypt entity blobs; keep keys off the untrusted client.
2. Narrator counter: Interest management + structural kill — mitigate_world: fog + no client key → structural_kill.
3. Step 1: multi-step mitigate — rotate key off client, encrypt, strict fog.
4. Step 2: score delayed-origin residual (still better than full leak).

Team / depth sensors:
- `depth::LeakageScorer`
- `server::Observer`
- `depth::EntityTruth`
- `depth::FogPolicy`
- `depth::StreamCryptoState`

Multi-reason / result fields and sensors:
- result field `key_stripped`
- result field `fog_applied`
- result field `structural_kill`
- result field `multi_reason`
- result field `detected`
- result field `mitigated`
- local `reasons` init=0
- structural fog / stream surfaces

Win conditions for this pair:
- detected := `false`
- mitigated := `after.structural_kill && r.key_stripped && r.fog_applied`
- Pass narrative: mitigation-only (fog/policy/structural) — detect may stay false; that is still a blue win.

## Takeaway

Blue can constrain or fog the advantage without a classic client scar detect. Check mitigated and server-side flags; structural lessons still count as blue wins.

## Run

```bash
./build/strategy_lab run 40_entity_stream_crypto
```

Optional filters:

```bash
./build/strategy_lab list
./build/strategy_lab run --tier all
```

Open the pair sources beside this lesson:

- `entity_stream_crypto/red_example.cpp` — full red multi-step
- `entity_stream_crypto/blue_example.cpp` — full blue multi-reason
- `entity_stream_crypto/pair.cpp` — StrategyEntry wiring + narrator

Related team / shared headers pulled by this pair:
- `depth/leakage_scorer.hpp`
