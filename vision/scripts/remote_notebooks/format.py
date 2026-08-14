"""Notebook cell builders and JSON writer."""

from __future__ import annotations

import json
from pathlib import Path


def _src(text: str) -> list[str]:
    lines = text.splitlines(keepends=True)
    if lines and not lines[-1].endswith("\n"):
        lines[-1] += "\n"
    if not lines:
        return ["\n"]
    return lines


def md(text: str) -> dict:
    return {"cell_type": "markdown", "metadata": {}, "source": _src(text)}


def code(text: str) -> dict:
    return {
        "cell_type": "code",
        "metadata": {},
        "source": _src(text),
        "outputs": [],
        "execution_count": None,
    }


def write_nb(path: Path, cell_list: list[dict]) -> None:
    doc = {
        "nbformat": 4,
        "nbformat_minor": 5,
        "metadata": {
            "kernelspec": {
                "display_name": "Python 3",
                "language": "python",
                "name": "python3",
            },
            "language_info": {"name": "python", "pygments_lexer": "ipython3"},
        },
        "cells": cell_list,
    }
    path.write_text(json.dumps(doc, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
    json.loads(path.read_text(encoding="utf-8"))
    print(f"wrote {path} ({len(cell_list)} cells)")
