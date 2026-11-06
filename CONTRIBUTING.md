# Contributing

Periscope is a monorepo of four independent research trees. Pick the subtree you change and follow that subtree’s rules.

## Which guide applies

| Touch | Follow |
|-------|--------|
| `anti-cheat/**` | [`anti-cheat/CONTRIBUTING.md`](anti-cheat/CONTRIBUTING.md) |
| `radar/**` | [`radar/CONTRIBUTING.md`](radar/CONTRIBUTING.md) |
| `vision/**` | [`vision/CONTRIBUTING.md`](vision/CONTRIBUTING.md) |
| `lens/**` | [`lens/CONTRIBUTING.md`](lens/CONTRIBUTING.md) |
| Root policy / CI only (`README.md`, `CONTRIBUTING.md`, `SECURITY.md`, `NOTICE.md`, root `.gitignore`, `.github/`) | Keep them accurate for all four trees; do not invent shared APIs that do not exist |

## Pull request hygiene

- One concern per PR. Do not mix unrelated anti-cheat, radar, vision and lens changes unless the change is truly shared infra (for example root docs or ignore rules).
- Say which subtree you touched and what you ran (build flags / test filters).
- Match style in the folder you edit. Do not “fix” the other tree in the same PR.

## Do not commit

- Virtualenvs (`.venv/`, `venv/`)
- Build trees (`anti-cheat/build*`, `radar/build*`, CMake cache, `*.obj` / `*.pdb` / `*.exe` droppings)
- Local model weights, ONNX dumps, Ultralytics `runs/`
- Private frames, gameplay video, machine-local configs (`cs2-vision-config.json`, tokens)
- Local development-tool caches and instruction files

Repository ignore rules should already cover these. If something still shows
up in `git status`, update the relevant rule rather than force-adding it.

## Checks

### Anti-cheat lab

From the repository root:

```bash
python3 scripts/verify.py anti-cheat-sim
```

Details: [`anti-cheat/CONTRIBUTING.md`](anti-cheat/CONTRIBUTING.md).

### Radar

From the repository root:

```bash
python3 scripts/verify.py radar-sim
```

Details: [`radar/docs/BUILD-AND-TEST.md`](radar/docs/BUILD-AND-TEST.md), [`radar/CONTRIBUTING.md`](radar/CONTRIBUTING.md).

### Vision

From `vision/`:

```bash
uv sync --extra dev
uv run ruff check src/ tests/
uv run ruff format --check src/ tests/
uv run mypy --strict src/cs2_vision_access/
uv run pytest tests/ -q
uv build
```

From the repository root, the same complete lane is
`python3 scripts/verify.py vision-cpu`.

### Lens

From `lens/`:

```bash
uv sync --extra dev
uv run ruff check src/ tests/
uv run ruff format --check src/ tests/
uv run mypy --strict src/periscope_lens/
uv run pytest tests/ -q
uv build
```

From the repository root, the same complete lane is
`python3 scripts/verify.py lens-cpu`. Run
`python3 scripts/verify.py architecture` after changing boundaries or moving
code.

The `tests/` directories of `anti-cheat/`, `radar/`, `vision/` and `lens/`
are tracked (the root `.gitignore` allowlists their test files). A fresh
clone can therefore run every lane without the local tree being present.

Details: [`vision/CONTRIBUTING.md`](vision/CONTRIBUTING.md).

## Ground rules (all trees)

- Keep work research- and accessibility-oriented, and document every capability you add (build flags, risk notes, architecture placement).
- Vision: current capture paths are pixel-based. Memory-access or input paths are permitted additions (see [decision 0002](docs/decisions/0002-live-game-use-permitted.md)) and belong behind adapters with updated docs.
- Anti-cheat: the lab's sources stay simulation-based (`sim::World`); real-process capability lives in `radar/`.
- Radar: simulation demos are the default path; real backends stay behind CMake flags and should degrade cleanly when hardware or the game is missing.
