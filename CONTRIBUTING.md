# Contributing

Periscope is a monorepo of two independent research trees. Pick the subtree you change and follow that subtree’s rules.

## Which guide applies

| Touch | Follow |
|-------|--------|
| `radar/**` | [`radar/CONTRIBUTING.md`](radar/CONTRIBUTING.md) |
| `vision/**` | [`vision/CONTRIBUTING.md`](vision/CONTRIBUTING.md) |
| Root policy / CI only (`README.md`, `CONTRIBUTING.md`, `SECURITY.md`, `NOTICE.md`, root `.gitignore`, `.github/`) | Keep them accurate for both trees; do not invent shared APIs that do not exist |

## Pull request hygiene

- **One concern per PR.** Do not mix unrelated radar and vision changes unless the change is truly shared infra (for example root docs or ignore rules).
- Say which subtree you touched and what you ran (build flags / test filters).
- Match style in the folder you edit. Do not “fix” the other tree in the same PR.

## Do not commit

- Virtualenvs (`.venv/`, `venv/`)
- Build trees (`radar/code/build*`, CMake cache, `*.obj` / `*.pdb` / `*.exe` droppings)
- Local model weights, ONNX dumps, Ultralytics `runs/`
- Private frames, gameplay video, machine-local configs (`cs2-vision-config.json`, tokens)
- Nested assistant caches (`.claude/`, `.cursor/`, `.codex/`, `.grok/`, …)

Root and per-subtree `.gitignore` files should already cover these. If something still shows up in `git status`, fix ignore rules rather than force-adding.

## Archives are not live truth

Historical ledgers and snapshots live only under:

- `radar/docs/archive/`
- `vision/docs/archive/`
- `vision/archive/`

Do not revive archive material as current architecture, CLI contract, or curriculum without rewriting it into active docs and code.

## Checks

### Radar

From `radar/code/`:

```bash
cmake -S . -B build -DLR_BUILD_TESTS=ON -DLR_BUILD_STRATEGY_LAB=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
# optional strategy catalog pass:
./build/strategy_lab all --quiet
```

Details: [`radar/docs/BUILD-AND-TEST.md`](radar/docs/BUILD-AND-TEST.md), [`radar/CONTRIBUTING.md`](radar/CONTRIBUTING.md).

### Vision

From `vision/`:

```bash
uv sync --extra dev
uv run ruff check src/ tests/
uv run ruff format --check src/ tests/
uv run mypy --strict src/cs2_vision_access/
uv run pytest tests/ -v \
  --ignore=tests/test_live_capture.py \
  --ignore=tests/test_live_pipeline.py \
  --ignore=tests/test_live_cli.py \
  -k "not gpu and not cuda"
```

Details: [`vision/CONTRIBUTING.md`](vision/CONTRIBUTING.md).

## Ground rules (both trees)

- Keep work educational / research / accessibility-oriented. Do not ship ready-to-run cheat product packaging aimed at live competitive play.
- Vision: no process memory access, no input injection, no aim assist.
- Radar: simulation is the default path; real backends stay behind CMake flags and should degrade cleanly when hardware or the game is missing.
