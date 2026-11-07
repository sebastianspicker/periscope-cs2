# NOTICE

Radar is MIT for the sources under `radar/`. External tools, SDKs, and any
game binaries you attach to in real mode are outside this tree and keep their
own terms. See [LICENSE](LICENSE) and [SECURITY.md](SECURITY.md).

Adapted upstream designs and code (2026-11-05 research pass):

- tiansongyu/cs2_cheat - MIT. The player-sample hygiene policies
  (`src/domain/radar/sampling.hpp`, ledger R2) are a port of
  `radar_player_sample.hpp`; the snapshot/protocol and fixed-map radar design
  (R1, R4, R5, R6, R7) follow its documented architecture.
- cs2-dma/CS2-DMA - Apache-2.0. The snapshot-ring/scene-watermark
  concurrency design (R3), the web-radar service shape and runtime stats
  (R5), and the offset-sync workflow hardening (R8) are adapted from its
  sources. Apache-2.0 NOTICE obligations are honored here.
- staromodny/miata - license verified per-tree before code reuse. The
  view-relative mini-radar math (R12) is a port of `src/main.cpp`.
- avitran0/deadlocked, dkrutsko/oasis, chao-shushu/CS2-DMA,
  Read1dno/vesta - designs referenced; verify each LICENSE before any
  code (not just design) is copied (ledger X1).
- IMXNOOBX/cs2-external-esp - CC BY-NC 4.0: reference-only, no code
  reuse (ledger X3).

The anti-cheat lab (`../anti-cheat/`) is original code with no adapted
upstreams: see [`../anti-cheat/NOTICE.md`](../anti-cheat/NOTICE.md).

## Second-pass design references (2026-11-06)

These trees were read for the second assessment pass. No code was copied
verbatim; the listed ideas were reimplemented in radar's style.

- `hyper-reV-main` (no license file): guest page walk and zero-run finder,
  shadow-hook entry bookkeeping, page-boundary hypercall chunking.
- `cs2ext-src` (no license file): live map-collision BVH extraction,
  weapon/name/spectator reads, cleanup handlers, timer resolution, system
  preflight.
- `velocity-cs2` (no license file): schema-scope resolver, signature DSL,
  encoded address registry, handle sentinels, cache-identity notes.
- `nexus` (no license file): single IOCTL envelope, JSON parser cautions.
- `CS2-GFusion-Python-main` (restrictive license, no derivatives without
  permission): `.opt` collision map format, world-entity scan budget,
  track TTL cautions. Treat its code as reference only.

Verify each upstream's terms again before copying code rather than an idea.
