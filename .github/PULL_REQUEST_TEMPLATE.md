## Subtree(s)

<!-- Which part of the monorepo does this touch? -->

- [ ] `anti-cheat/`
- [ ] `radar/`
- [ ] `vision/`
- [ ] `lens/`
- [ ] monorepo meta (root docs, `.github/`, top-level layout)

## Summary

<!-- What changed and why (a few sentences). -->

## Test plan

<!-- Check what you ran. Paths are from the monorepo root unless noted. -->

**anti-cheat** (from repo root):

- [ ] `python3 scripts/verify.py anti-cheat-sim`

**radar** (from repo root):

- [ ] `python3 scripts/verify.py radar-sim`

**vision** (from `vision/`; the root lane is `python3 scripts/verify.py vision-cpu`):

- [ ] `uv run ruff check src/ tests/`
- [ ] `uv run ruff format --check src/ tests/`
- [ ] `uv run mypy --strict src/cs2_vision_access/`
- [ ] `uv run pytest tests/ -q`
- [ ] `uv build`
- [ ] Extra checks if relevant (live, GPU, train path, notebook regen)

**lens** (from `lens/`; the root lane is `python3 scripts/verify.py lens-cpu`):

- [ ] `uv run ruff check src/ tests/`
- [ ] `uv run ruff format --check src/ tests/`
- [ ] `uv run mypy --strict src/periscope_lens/`
- [ ] `uv run pytest tests/ -q`
- [ ] `uv build`

**architecture** (from repo root):

- [ ] `python3 scripts/check_architecture.py`

## Safety checklist

- [ ] No private gameplay frames, credentials, or local model weights committed
- [ ] **anti-cheat:** stays simulation-based (`sim::World`); real-process capability belongs in `radar/`
- [ ] **radar:** real-backend changes documented with build flags and risk notes
- [ ] **vision:** new capabilities (memory, input) placed behind adapters and documented
- [ ] **lens:** actuation paths opt-in and documented
- [ ] Docs updated if CLI, config, or build behavior changed
- [ ] No breaking public surface change, or called out above with migration notes
