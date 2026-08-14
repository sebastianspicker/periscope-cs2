"""Library API for Vombit+EdgeSAM prepare (callable from train-auto, not only CLI).

Provides :func:`run_cs2_sam_prepare` for video or existing-image labeling,
:func:`discover_edgesam_assets` / :func:`ensure_edgesam_assets` for teacher
ONNX packs, and :func:`bootstrap_with_edgesam` for shared image-dir bootstrap
orchestration used by remote autonomous and train-auto label stages.
"""

from __future__ import annotations

# Re-export patch targets used by tests (module-level attributes).
import cv2 as cv2

from cs2_vision_access.segmenters.cs2_sam import Cs2SamSegmenter as Cs2SamSegmenter
from cs2_vision_access.training.prepare_lib.assets import (
    discover_edgesam_assets,
    ensure_edgesam_assets,
)
from cs2_vision_access.training.prepare_lib.bootstrap import (
    EdgesamBootstrapResult,
    bootstrap_with_edgesam,
    candidate_image_dirs_for_labeling,
)
from cs2_vision_access.training.prepare_lib.prepare_run import (
    PrepareResult,
    run_cs2_sam_prepare,
)

__all__ = [
    "EdgesamBootstrapResult",
    "PrepareResult",
    "bootstrap_with_edgesam",
    "candidate_image_dirs_for_labeling",
    "discover_edgesam_assets",
    "ensure_edgesam_assets",
    "run_cs2_sam_prepare",
]
