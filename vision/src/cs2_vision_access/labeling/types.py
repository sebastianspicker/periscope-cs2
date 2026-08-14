"""Types, constants, and class-map parsing for box-to-mask bootstrap."""

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass

SCHEMA_VERSION = 1
DRAFT_STATUS_FILENAME = "draft_status.json"
REVIEW_STATUS_DRAFT_PENDING = "draft_pending"
REVIEW_STATUS_ACCEPTED = "accepted"
REVIEW_STATUS_REJECTED = "rejected"
REVIEW_STATUS_PROMOTED = "promoted"
# schema_version 1 allowed review_status values (per-file and batch-level).
REVIEW_STATUSES = frozenset(
    {
        REVIEW_STATUS_DRAFT_PENDING,
        REVIEW_STATUS_ACCEPTED,
        REVIEW_STATUS_REJECTED,
        REVIEW_STATUS_PROMOTED,
    }
)
DEFAULT_OUTPUT_CLASS_ID = 0
ELLIPSE_POINT_COUNT = 16
SUPPORTED_BACKENDS = frozenset({"rectangle", "ellipse", "sam"})
DEFAULT_BACKEND = "rectangle"

# Common CS:GO / CS2 detection class names → source class ids (keremberke-style).
_KNOWN_SOURCE_CLASS_NAMES: dict[str, int] = {
    "ct": 0,
    "t": 1,
    "cthead": 2,
    "ct_head": 2,
    "thead": 3,
    "t_head": 3,
}


class BootstrapError(ValueError):
    """Box-to-mask conversion cannot proceed safely."""


@dataclass(frozen=True)
class DetectionBox:
    """One YOLO detection row in normalized coordinates."""

    source_class_id: int
    x_center: float
    y_center: float
    width: float
    height: float
    line_number: int


@dataclass(frozen=True)
class BootstrapSummary:
    images_dir: str
    labels_dir: str
    output_labels_dir: str
    backend: str
    image_count: int
    label_count: int
    instance_count: int
    negative_count: int
    draft_status_path: str
    review_status: str

    def as_dict(self) -> dict[str, object]:
        return {
            "images_dir": self.images_dir,
            "labels_dir": self.labels_dir,
            "output_labels_dir": self.output_labels_dir,
            "backend": self.backend,
            "image_count": self.image_count,
            "label_count": self.label_count,
            "instance_count": self.instance_count,
            "negative_count": self.negative_count,
            "draft_status_path": self.draft_status_path,
            "review_status": self.review_status,
            "schema_version": SCHEMA_VERSION,
        }


@dataclass(frozen=True)
class ClassMap:
    """Map source detection class ids to output segmentation class ids.

    When ``default_to`` is set (default bootstrap), every source class maps to
    that id. When ``mapping`` is set, only listed source ids are accepted.
    """

    mapping: Mapping[int, int] | None = None
    default_to: int | None = DEFAULT_OUTPUT_CLASS_ID

    def resolve(self, source_class_id: int) -> int:
        if self.mapping is not None:
            if source_class_id not in self.mapping:
                raise BootstrapError(
                    f"source class id {source_class_id} is not listed in --class-map"
                )
            return self.mapping[source_class_id]
        if self.default_to is None:
            raise BootstrapError("class map has neither mapping nor default_to")
        return self.default_to

    def as_json(self) -> dict[str, object]:
        if self.mapping is not None:
            return {
                "mode": "explicit",
                "entries": {str(source): target for source, target in sorted(self.mapping.items())},
            }
        return {"mode": "default_all", "default_to": self.default_to}


def parse_class_map(spec: str | None) -> ClassMap:
    """Parse ``ct=0,t=0,0=0`` style maps into a :class:`ClassMap`.

    Keys may be decimal source class ids or known CSGO class names
    (``ct``, ``t``, ``cthead``, ``thead``, …). Values are non-negative
    integer output class ids. Omitting the map defaults every class to ``0``.
    """
    if spec is None or not str(spec).strip():
        return ClassMap(mapping=None, default_to=DEFAULT_OUTPUT_CLASS_ID)

    mapping: dict[int, int] = {}
    unknown_name_order = 0
    for raw_part in str(spec).split(","):
        part = raw_part.strip()
        if not part:
            raise BootstrapError("class-map has an empty entry")
        key_text, separator, value_text = part.partition("=")
        if not separator:
            raise BootstrapError(f"invalid class-map entry {part!r}; expected SOURCE=TARGET")
        key = key_text.strip()
        value = value_text.strip()
        if not key or not value:
            raise BootstrapError(f"invalid class-map entry {part!r}; expected SOURCE=TARGET")
        if not value.isascii() or not value.isdecimal():
            raise BootstrapError(
                f"invalid class-map target {value!r}; expected non-negative integer"
            )
        target = int(value)
        if key.isascii() and key.isdecimal():
            source_id = int(key)
        else:
            known = _KNOWN_SOURCE_CLASS_NAMES.get(key.casefold())
            if known is not None:
                source_id = known
            else:
                # Unknown names get sequential ids in declaration order so
                # custom maps still work without a data.yaml.
                source_id = unknown_name_order
                unknown_name_order += 1
        if source_id in mapping and mapping[source_id] != target:
            raise BootstrapError(f"conflicting class-map entries for source class {source_id}")
        mapping[source_id] = target
    if not mapping:
        raise BootstrapError("class-map must list at least one SOURCE=TARGET pair")
    return ClassMap(mapping=mapping, default_to=None)


def parse_backend(value: str) -> str:
    normalized = value.strip().casefold()
    if normalized not in SUPPORTED_BACKENDS:
        supported = ", ".join(sorted(SUPPORTED_BACKENDS))
        raise BootstrapError(f"unknown backend {value!r}; supported: {supported}")
    return normalized
