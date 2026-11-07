"""Model-governance hygiene gate (V9).

Every entry in ``MODEL_REGISTRY`` is a promise to the operator: the pinned
SHA-256 must be well-formed, the license recorded, and the source URL
HTTPS. A new registry entry that skips any of these silently reintroduces
the A6 incident class (an unverified model running as if it were verified).

The explicit ``sam_*_path`` / ``CS2_SAM_*`` override path is a deliberate
custom export (A6 judgement call): it must
exist and is logged as unpinned, this gate covers the *registry*, which is
the default trust path.
"""

from __future__ import annotations

import re

from cs2_vision_access.application.configuration.data.model_registry import (
    MODEL_REGISTRY,
)

_SHA256 = re.compile(r"^[0-9a-f]{64}$")


def test_every_registry_entry_is_fully_pinned() -> None:
    assert MODEL_REGISTRY, "the registry must not be empty"
    for name, entry in MODEL_REGISTRY.items():
        sha256 = entry.get("sha256")
        assert isinstance(sha256, str) and _SHA256.fullmatch(sha256), (
            f"{name}: sha256 must be 64 lowercase hex characters"
        )
        license_name = entry.get("license")
        assert isinstance(license_name, str) and license_name, f"{name}: license must be recorded"
        url = entry.get("url")
        assert isinstance(url, str) and url.startswith("https://"), (
            f"{name}: source url must be https"
        )
        task = entry.get("task")
        assert isinstance(task, str) and task, f"{name}: task must be recorded"


def test_registry_license_notes_match_notice() -> None:
    """Ultralytics-family entries are AGPL; Vombit weights are CC-BY-NC-ND.

    If a license tag changes here, vision/NOTICE.md and
    vision/docs/MODEL-GOVERNANCE.md must change in the same commit, the
    governance doc is the human-readable half of this gate.
    """
    licenses = {entry.get("license") for entry in MODEL_REGISTRY.values()}
    assert "AGPL-3.0" in licenses, "ultralytics weights stay AGPL-3.0"
    assert "CC-BY-NC-ND-4.0" in licenses, "vombit weights stay CC-BY-NC-ND-4.0"
    assert "NTU-S-Lab-1.0" in licenses, "edgesam is NTU S-Lab 1.0, not MIT (verified 2026-11-06)"


def test_registry_entry_names_are_stable_identifiers() -> None:
    """Names are CLI identifiers (`download-model <name>`); keep them
    lowercase-kebab so docs and shell completion never quote them."""
    for name in MODEL_REGISTRY:
        assert re.fullmatch(r"[a-z0-9][a-z0-9-]*", name), f"bad name: {name}"
