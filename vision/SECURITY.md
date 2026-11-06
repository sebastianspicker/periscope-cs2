# Security

This file covers the **vision** track (`cs2-vision-access` / `cs2-vision`) inside the Periscope monorepo. Report vulnerabilities privately to the repository owner once a public contact channel is published (monorepo root or this track). Until then, do not open public issues that include:

- Gameplay frames with personal or account data
- Private model weights or API tokens
- Credentials or exploit payloads

## Operating assumptions

- This software captures external pixels (screen or capture device) or reads local video files. Game-memory access and input automation are permitted directions ([../docs/decisions/0002-live-game-use-permitted.md](../docs/decisions/0002-live-game-use-permitted.md)) that this track does not implement yet.
- Operators are responsible for compliance with platform terms, anti-cheat policies, and local law.
- Third-party datasets and base weights carry their own licenses (for example Ultralytics AGPL defaults, CS2-10k CC BY-NC 4.0).
- The sister `radar/` track is a separate track with different capabilities and license; do not assume vision-track capabilities apply there.

## Model integrity

Prefer loading ONNX files together with checksum manifests from `download-model`, `train`, or `register-model`. Treat mismatched hashes as untrusted.
