"""Shared CLI helpers."""

from __future__ import annotations

import json
from pathlib import Path


def _parse_classes(values: list[str]) -> dict[int, str]:
    parsed: dict[int, str] = {}
    for value in values:
        class_id_text, separator, name = value.partition("=")
        if (
            not separator
            or not class_id_text.isascii()
            or not class_id_text.isdecimal()
            or not name.strip()
        ):
            raise ValueError(f"invalid class mapping {value!r}; expected ID=NAME")
        class_id = int(class_id_text)
        if class_id in parsed:
            raise ValueError(f"duplicate class id: {class_id}")
        parsed[class_id] = name.strip()
    if set(parsed) != set(range(len(parsed))):
        raise ValueError("class ids must be contiguous and start at zero")
    return dict(sorted(parsed.items()))


def _load_classes_json(path: Path) -> dict[int, str]:
    if path.is_symlink() or not path.is_file() or path.suffix.lower() != ".json":
        raise ValueError("classes JSON must be a regular local .json file")
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"could not read classes JSON: {error}") from error
    if not isinstance(payload, dict):
        raise ValueError("classes JSON root must be an object")
    if any(
        not isinstance(key, str) or not isinstance(value, str) for key, value in payload.items()
    ):
        raise ValueError("classes JSON keys and values must be strings")
    values = [f"{key}={value}" for key, value in payload.items()]
    return _parse_classes(values)


def _print_json(value: object) -> None:
    print(json.dumps(value, indent=2, sort_keys=True))
