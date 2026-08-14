## Subtree(s)

<!-- Which part of the monorepo does this touch? -->

- [ ] `radar/`
- [ ] `vision/`
- [ ] monorepo meta (root docs, `.github/`, top-level layout)

## Summary

<!-- What changed and why (a few sentences). -->

## Test plan

<!-- Check what you ran. Paths are from the monorepo root. -->

**vision** (from `vision/`):

- [ ] `uv run ruff check src/ tests/`
- [ ] `uv run pytest tests/ -v --ignore=tests/test_live_capture.py --ignore=tests/test_live_pipeline.py --ignore=tests/test_live_cli.py -k "not gpu and not cuda"`
- [ ] Extra checks if relevant (live, GPU, train path, notebook regen)

**radar** (from repo root, or `cd radar/code`):

- [ ] `cmake -S radar/code -B radar/code/build -DLR_BUILD_TESTS=ON -DLR_BUILD_STRATEGY_LAB=ON`
- [ ] `cmake --build radar/code/build -j`
- [ ] `ctest --test-dir radar/code/build --output-on-failure`

## Safety checklist

- [ ] No private gameplay frames, credentials, or local model weights committed
- [ ] **vision:** remains external-capture / local-video only — no process memory, injection, or simulated game input
- [ ] **radar:** educational / simulation scope only; no live-game weaponization payloads
- [ ] Docs updated if CLI, config, or build behavior changed
- [ ] No breaking public surface change, **or** called out above with migration notes
