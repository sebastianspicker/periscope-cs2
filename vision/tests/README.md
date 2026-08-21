# Tests

Focused pytest contracts for `cs2_vision_access`. Config: `pyproject.toml` → `[tool.pytest.ini_options]`.

## Layout

```text
tests/
├── test_*.py             # safety, model, provider, and data-integrity contracts
└── README.md
```

## Run

```bash
uv run pytest tests/
```

Repo-root CI (`.github/workflows/vision-ci.yml`, `working-directory: vision`) runs this suite
on Ubuntu and Windows for Python 3.11–3.13. It does not run real Ultralytics epochs or hardware overlay validation.

## Coverage by domain

The suite retains file safety, model manifests, trusted provider discovery, and dataset integrity
contracts. Input data is created programmatically in temporary directories. Generated outputs,
coverage reports, and caches are gitignored (`vision/.gitignore` and monorepo ignore rules as
applicable).
