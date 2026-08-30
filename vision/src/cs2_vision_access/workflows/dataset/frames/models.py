"""Frame extraction data models — provenance, policy, summaries, and validation.

Separated from ``extract.py`` so that model consumers do not depend on
OpenCV or the extraction call chain.
"""

from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path

SESSION_FILENAME = "session.json"
SCHEMA_VERSION = 1
RIGHTS_STATUS_PLACEHOLDER = "placeholder"


_SESSION_KEYS = frozenset(
    {
        "schema_version",
        "session_id",
        "source_stem",
        "rights",
        "capture_notes",
        "frame_policy",
    }
)
_RIGHTS_KEYS = frozenset(
    {
        "status",
        "source_identifier",
        "consent_record",
        "redistribution",
        "retention_notes",
    }
)
_FRAME_POLICY_KEYS = frozenset(
    {
        "every_n_frames",
        "max_saved_frames",
        "decoded_frames",
        "saved_frames",
    }
)


class FrameExtractionError(RuntimeError):
    """Frame extraction failed."""


class SessionProvenanceError(ValueError):
    """session.json provenance sidecar failed schema validation."""


@dataclass(frozen=True)
class RightsPlaceholder:
    """Rights/consent metadata embedded in session provenance.

    Complete schema fields for operator-recorded rights. The default status
    value ``RIGHTS_STATUS_PLACEHOLDER`` means rights have not been filled in
    yet; operators should set a concrete status (e.g. consented) before share.
    """

    status: str
    source_identifier: str
    consent_record: str
    redistribution: str
    retention_notes: str

    @classmethod
    def placeholder(cls) -> RightsPlaceholder:
        """Factory for an empty rights record with status ``placeholder``."""
        return cls(
            status=RIGHTS_STATUS_PLACEHOLDER,
            source_identifier="",
            consent_record="",
            redistribution="",
            retention_notes="",
        )


@dataclass(frozen=True)
class FramePolicy:
    """Record of what was extracted and what was requested."""

    every_n_frames: int
    max_saved_frames: int
    decoded_frames: int
    saved_frames: int


@dataclass(frozen=True)
class SessionProvenance:
    """Schema-versioned metadata sidecar written beside extracted frames."""

    schema_version: int
    session_id: str
    source_stem: str
    rights: RightsPlaceholder
    capture_notes: str
    frame_policy: FramePolicy

    def as_json(self) -> dict[str, object]:
        return {
            "schema_version": self.schema_version,
            "session_id": self.session_id,
            "source_stem": self.source_stem,
            "rights": asdict(self.rights),
            "capture_notes": self.capture_notes,
            "frame_policy": asdict(self.frame_policy),
        }

    @classmethod
    def load(cls, path: str | Path) -> SessionProvenance:
        session_path = Path(path)
        if session_path.is_symlink():
            raise SessionProvenanceError("session.json must not be a symlink")
        if not session_path.is_file():
            raise SessionProvenanceError(f"session.json is not a file: {session_path}")
        try:
            raw = json.loads(session_path.read_text(encoding="utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
            raise SessionProvenanceError(f"could not read session.json: {error}") from error
        return cls.from_mapping(raw)

    @classmethod
    def from_mapping(cls, raw: object) -> SessionProvenance:
        if not isinstance(raw, dict):
            raise SessionProvenanceError("session.json root must be a JSON object")
        keys = frozenset(raw)
        if keys != _SESSION_KEYS:
            missing = sorted(_SESSION_KEYS - keys)
            unknown = sorted(keys - _SESSION_KEYS)
            raise SessionProvenanceError(
                f"session.json keys do not match schema; missing={missing}, unknown={unknown}"
            )
        return cls(
            schema_version=_require_schema_version(raw["schema_version"]),
            session_id=_require_nonempty_text(raw["session_id"], "session_id"),
            source_stem=_require_nonempty_text(raw["source_stem"], "source_stem"),
            rights=_parse_rights(raw["rights"]),
            capture_notes=_require_text(raw["capture_notes"], "capture_notes"),
            frame_policy=_parse_frame_policy(raw["frame_policy"]),
        )


@dataclass(frozen=True)
class FrameExtractionSummary:
    """Result summary returned by ``extract_frames``."""

    decoded_frames: int
    saved_frames: int
    output_directory: str
    session_id: str
    source_stem: str
    session_path: str


# ------------------------------------------------------------------
# Validation helpers (extracted from the original frames.py monolith)
# ------------------------------------------------------------------


def _require_schema_version(value: object) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value != SCHEMA_VERSION:
        raise SessionProvenanceError(f"schema_version must be {SCHEMA_VERSION}")
    return value


def _require_nonempty_text(value: object, field: str) -> str:
    text = _require_text(value, field)
    if not text.strip():
        raise SessionProvenanceError(f"{field} must be a non-empty string")
    return text


def _require_text(value: object, field: str) -> str:
    if not isinstance(value, str):
        raise SessionProvenanceError(f"{field} must be a string")
    return value


def _require_positive_int(value: object, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise SessionProvenanceError(f"{field} must be a positive integer")
    return value


def _require_nonnegative_int(value: object, field: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < 0:
        raise SessionProvenanceError(f"{field} must be a non-negative integer")
    return value


def _parse_rights(raw: object) -> RightsPlaceholder:
    if not isinstance(raw, dict):
        raise SessionProvenanceError("rights must be a JSON object")
    keys = frozenset(raw)
    if keys != _RIGHTS_KEYS:
        missing = sorted(_RIGHTS_KEYS - keys)
        unknown = sorted(keys - _RIGHTS_KEYS)
        raise SessionProvenanceError(
            f"rights keys do not match schema; missing={missing}, unknown={unknown}"
        )
    return RightsPlaceholder(
        status=_require_nonempty_text(raw["status"], "rights.status"),
        source_identifier=_require_text(raw["source_identifier"], "rights.source_identifier"),
        consent_record=_require_text(raw["consent_record"], "rights.consent_record"),
        redistribution=_require_text(raw["redistribution"], "rights.redistribution"),
        retention_notes=_require_text(raw["retention_notes"], "rights.retention_notes"),
    )


def _parse_frame_policy(raw: object) -> FramePolicy:
    if not isinstance(raw, dict):
        raise SessionProvenanceError("frame_policy must be a JSON object")
    keys = frozenset(raw)
    if keys != _FRAME_POLICY_KEYS:
        missing = sorted(_FRAME_POLICY_KEYS - keys)
        unknown = sorted(keys - _FRAME_POLICY_KEYS)
        raise SessionProvenanceError(
            f"frame_policy keys do not match schema; missing={missing}, unknown={unknown}"
        )
    every_n = _require_positive_int(raw["every_n_frames"], "frame_policy.every_n_frames")
    max_saved = _require_positive_int(raw["max_saved_frames"], "frame_policy.max_saved_frames")
    decoded = _require_nonnegative_int(raw["decoded_frames"], "frame_policy.decoded_frames")
    saved = _require_nonnegative_int(raw["saved_frames"], "frame_policy.saved_frames")
    if saved > decoded:
        raise SessionProvenanceError(
            "frame_policy.saved_frames cannot exceed frame_policy.decoded_frames"
        )
    if saved > max_saved:
        raise SessionProvenanceError(
            "frame_policy.saved_frames cannot exceed frame_policy.max_saved_frames"
        )
    return FramePolicy(
        every_n_frames=every_n,
        max_saved_frames=max_saved,
        decoded_frames=decoded,
        saved_frames=saved,
    )
