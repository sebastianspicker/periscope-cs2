"""Fill strategies for rendered instance masks — tint overlay and halo glow.

Extracted from ``draw.py`` so the ``OutlineRenderer`` class focuses on
contour acceptance and rendering dispatch, not compositing arithmetic.
"""

from __future__ import annotations

from typing import Any

import numpy as np

from cs2_vision_access.domain.outline import (
    DRAW_COORDINATE_SHIFT,
    OutlineStyle,
    parse_hex_bgr,
)


def apply_tint_fill(
    cv2: Any,
    output: np.ndarray,
    contours: list[np.ndarray],
    style: OutlineStyle,
) -> np.ndarray:
    """Hard interior tint using inner color at ``fill_opacity``.

    Composites a solid-colour polygon fill over the frame via
    ``cv2.addWeighted``.
    """
    overlay = output.copy()
    cv2.fillPoly(
        overlay,
        contours,
        parse_hex_bgr(style.inner_color),
        shift=DRAW_COORDINATE_SHIFT,
    )
    return np.asarray(
        cv2.addWeighted(overlay, style.fill_opacity, output, 1.0 - style.fill_opacity, 0),
        dtype=np.uint8,
    )


def apply_halo_fill(
    cv2: Any,
    output: np.ndarray,
    contours: list[np.ndarray],
    style: OutlineStyle,
) -> np.ndarray:
    """Soft static glow — rasterize, dilate, blur, composite outer color.

    ``fill_opacity`` is glow strength in [0, 0.35].  Dual-stroke outlines
    are drawn separately on top by the caller.
    """
    height, width = output.shape[:2]
    mask = np.zeros((height, width), dtype=np.uint8)
    cv2.fillPoly(mask, contours, 255, shift=DRAW_COORDINATE_SHIFT)

    blur = int(style.halo_blur)
    if blur % 2 == 0:
        blur += 1
    kernel_span = max(3, blur)

    structuring = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (kernel_span, kernel_span))
    expanded = cv2.dilate(mask, structuring)
    blurred = cv2.GaussianBlur(expanded, (blur, blur), 0)

    alpha = (blurred.astype(np.float32) / 255.0) * float(style.fill_opacity)
    color = np.array(parse_hex_bgr(style.outer_color), dtype=np.float32)

    out_f = output.astype(np.float32)
    for channel in range(3):
        out_f[:, :, channel] = out_f[:, :, channel] * (1.0 - alpha) + color[channel] * alpha
    return np.clip(out_f, 0, 255).astype(np.uint8)
