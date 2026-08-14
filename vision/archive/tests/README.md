# Archived tests

Obsolete test modules kept for historical reference. They are not collected by
pytest (`testpaths = ["tests"]` in `pyproject.toml`).

## obsolete-shims/

Former re-export wrappers that pointed at domain-specific test modules:

- `test_cli.py` -> `test_cli_train`, `test_cli_outline`, `test_cli_prefs`, `test_cli_dataset`
- `test_evaluation.py` -> `test_evaluation_cli`, `test_evaluation_metrics`, `test_evaluation_yolo`
- `test_renderer.py` -> `test_renderer_outline`, `test_renderer_roles`
- `test_video.py` -> `test_video_loop`, `test_video_statistics`
- `test_forbidden_capabilities.py` -> emptied (no static ban list in this project)

Active tests live under `/tests/`.
