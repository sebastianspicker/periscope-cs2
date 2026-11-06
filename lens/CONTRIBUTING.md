# Contributing - Periscope Lens

- Keep the package self-contained: `src/periscope_lens/` imports numpy and
  its own modules only (no imports from `vision/` or `radar/`). The
  architecture check enforces this.
- Every module change lands with unit tests in `tests/`; there is no
  hardware in the loop, so tests use synthetic frames, streams and sockets.
- Run the full lane before proposing a change:

  ```bash
  cd lens
  uv sync --frozen --extra dev
  uv run ruff check src/ tests/
  uv run ruff format --check src/ tests/
  uv run mypy --strict src/periscope_lens/
  uv run pytest tests/ -q
  ```

- The Android drop follows the same rule at its own level: pure Kotlin only
  in encoders/helpers, gap notes when something is not wired.
- Never claim runtime verification that did not happen; use “runtime-
  unverified” explicitly (house style).
