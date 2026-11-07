# Periscope

Periscope is a set of four CS2 research projects with different threat models and licenses. They are research tools you build and configure yourself, and targeting a live, running game is in scope ([decision 0002](docs/decisions/0002-live-game-use-permitted.md)).

| If you want to, ... | Start here | What you get | Status |
|---|---|---|---|
| see players on a map while the game runs | [`radar/`](radar/) | External radar with optional Windows real backends, a local web view and a relay | Usable on Windows with real backends enabled; everything defaults to simulation |
| put outlines on visible players | [`vision/`](vision/) | Pixel-only capture and outline package with a CLI | Pre-alpha, CPU-first, HDMI second-device path is Windows-only |
| use a phone camera as the sensor | [`lens/`](lens/) | Calibration, optical checks, USB target protocol and an Android source drop | Research stage; no end-to-end live product yet |
| learn how external cheats and their counters work | [`anti-cheat/`](anti-cheat/) | Simulation-only red/blue lessons across five tiers | Maintained as a teaching lab |

Read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) before building anything. It maps the run paths, dependencies and CI.

## What each tree does

| Tree | Input | Runs on | Output | Default |
|---|---|---|---|---|
| [`anti-cheat/`](anti-cheat/) | A simulated world | Any C++20 host | Console lessons and pass/fail results | Simulation only |
| [`radar/`](radar/) | Game process memory or a simulated world | Windows for real backends, any host for simulation | Radar overlay, web radar, relay | Simulation; real backends behind CMake flags |
| [`vision/`](vision/) | Screen pixels, capture device or video file | Python 3.11 to 3.13 | Window, overlay, video file or shared-memory ring | CPU inference with ONNX Runtime |
| [`lens/`](lens/) | Phone camera frames | Python package plus an Android companion | Calibrated targets, motion deltas | Camera only; output paths are opt-in |

Radar consumes anti-cheat libraries one way. The lab never imports Radar. Vision and Lens share no runtime code with either.

## Quick start

Anti-cheat, from the repository root:

```bash
cmake -S anti-cheat -B anti-cheat/build/dev
cmake --build anti-cheat/build/dev -j
```

Radar, from the repository root. It needs the sibling anti-cheat checkout:

```bash
cmake -S radar -B radar/build/dev
cmake --build radar/build/dev -j
```

Vision, from `vision/`:

```bash
uv venv --python 3.12
uv sync --frozen --extra dev
uv run cs2-vision --help
```

Lens, from `lens/`:

```bash
uv venv --python 3.12
uv sync --frozen --extra dev
uv run pytest tests/ -q
```

Each tree has its own README with the full build and runtime notes.

## State of the project

This is independent exploratory work developed alongside a day job. The anti-cheat lab runs on a simulated world. Radar's real backends are opt-in and default to off. Vision and Lens are pre-alpha. The documented experiments do not yet establish a polished live-game product.

Live game usage is in scope (see [decision 0002](docs/decisions/0002-live-game-use-permitted.md)); follow the usage notes in each tree. Pixel-based perception can miss or misidentify visible content. None of this guarantees accessibility or anti-cheat acceptance. You are responsible for capture, process access and platform rules.

## Repository layout

```
.
├── README.md           This file
├── CONTRIBUTING.md     Contribution rules
├── SECURITY.md         Scope and reporting
├── NOTICE.md           License map and third-party pointers
├── docs/               Architecture and decisions
├── scripts/            Repository-wide checks
├── anti-cheat/         Simulation lab (MIT)
├── radar/              Radar product (MIT)
├── vision/             Pixel-only package (AGPL-3.0-only)
└── lens/               Phone-camera track (AGPL-3.0-only)
```

Every tree keeps its own README, LICENSE, CONTRIBUTING, SECURITY and docs. Root files describe the umbrella and do not replace them.

Run all portable checks with:

```bash
python3 scripts/verify.py all
```

## Guides

| Document | Contents |
|---|---|
| [CONTRIBUTING.md](CONTRIBUTING.md) | PR rules and per-tree verification commands |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Run paths, dependency rules, CI and code map |
| [SECURITY.md](SECURITY.md) | Reporting and scope |
| [NOTICE.md](NOTICE.md) | License map and third-party notes |
| [anti-cheat/README.md](anti-cheat/README.md) · [anti-cheat/docs/INDEX.md](anti-cheat/docs/INDEX.md) | Lab curriculum and build |
| [radar/README.md](radar/README.md) · [radar/docs/INDEX.md](radar/docs/INDEX.md) | Radar build, offsets and backends |
| [vision/README.md](vision/README.md) · [vision/docs/](vision/docs/) | Vision install and CLI |
| [lens/README.md](lens/README.md) · [lens/docs/TOPOLOGY.md](lens/docs/TOPOLOGY.md) | Lens build, topology and output support |

## Licensing

There is no single repository license. Each tree keeps its own:

- `anti-cheat/` is MIT, see `anti-cheat/LICENSE`
- `radar/` is MIT, see `radar/LICENSE`
- `vision/` is AGPL-3.0-only, see `vision/LICENSE` and `vision/NOTICE.md`
- `lens/` is AGPL-3.0-only, see `lens/LICENSE` and `lens/NOTICE.md`

Root policy files are packaging, not a relicense of any tree.
