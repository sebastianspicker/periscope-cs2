"""Field-debug overlay (ledger V6): boxes, labels, and timing on the composite.

A diagnostic surface, never a default: it is rendered only when
``LivePipelineConfig.debug_viz`` is set (CLI ``--debug-viz``). Draw order and
text content are deterministic so a golden-frame test can assert them.

The style resolves through the same rules the outline renderer uses where a
catalog is available; debug rectangles are always solid cyan-on-dark so they
remain readable over any outline style.
"""

from __future__ import annotations

from collections.abc import Sequence

import numpy as np

from cs2_vision_access.domain.predictions import InstanceMask

_DEBUG_BOX_BGR = (255, 255, 0)  # cyan in BGR
_DEBUG_TEXT_BGR = (255, 255, 255)
_DEBUG_HUD_BGR = (60, 220, 60)


def draw_debug_overlay(
    frame_bgr: np.ndarray,
    predictions: Sequence[InstanceMask],
    *,
    fps: float,
    avg_inference_ms: float,
    frame_index: int,
) -> np.ndarray:
    """Draws axis-aligned debug rectangles, per-instance labels and a HUD
    line onto ``frame_bgr`` **in place** and returns it.

    Deterministic: same inputs → same pixels (no timestamps in the HUD).
    """
    import cv2

    height, width = frame_bgr.shape[:2]
    font = cv2.FONT_HERSHEY_SIMPLEX
    font_scale = max(0.3, min(height, width) / 1280.0)
    thickness = max(1, round(font_scale * 2))

    for instance in predictions:
        xs = [point[0] for point in instance.polygon]
        ys = [point[1] for point in instance.polygon]
        x0 = max(0, int(min(xs)))
        y0 = max(0, int(min(ys)))
        x1 = min(width - 1, int(max(xs)))
        y1 = min(height - 1, int(max(ys)))
        cv2.rectangle(frame_bgr, (x0, y0), (x1, y1), _DEBUG_BOX_BGR, thickness)
        label = f"{instance.class_name} {instance.confidence:.2f}"
        cv2.putText(
            frame_bgr,
            label,
            (x0, max(0, y0 - 2)),
            font,
            font_scale,
            _DEBUG_TEXT_BGR,
            thickness,
            cv2.LINE_AA,
        )

    hud = f"f{frame_index} {fps:.1f}fps infer {avg_inference_ms:.1f}ms"
    cv2.putText(
        frame_bgr,
        hud,
        (4, max(12, int(14 * font_scale))),
        font,
        font_scale,
        _DEBUG_HUD_BGR,
        thickness,
        cv2.LINE_AA,
    )
    return frame_bgr
