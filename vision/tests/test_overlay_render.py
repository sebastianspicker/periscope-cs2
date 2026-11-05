"""Overlay/renderer correctness checks that run without Win32 or Cocoa."""

from __future__ import annotations

import ast
from pathlib import Path

import cv2
import numpy as np

import cs2_vision_access.adapters.capture.overlay_backends.win32 as win32_backend
from cs2_vision_access.adapters.capture.alpha_renderer import AlphaRenderer, AlphaRendererConfig
from cs2_vision_access.adapters.rendering.renderer.distance_draw import _draw_distance_outline
from cs2_vision_access.adapters.rendering.renderer.fills import apply_halo_fill, apply_tint_fill
from cs2_vision_access.adapters.rendering.renderer.jfa import _draw_jfa_outline
from cs2_vision_access.adapters.rendering.renderer.roles import RoleTreatment, TreatmentCatalog
from cs2_vision_access.domain.outline import DRAW_COORDINATE_SCALE, OutlineStyle
from cs2_vision_access.domain.predictions import InstanceMask

_CAPTURE = Path(win32_backend.__file__).parents[1]


def _binding_keys_used(source: str) -> set[str]:
    keys: set[str] = set()
    for node in ast.walk(ast.parse(source)):
        if (
            isinstance(node, ast.Subscript)
            and isinstance(node.slice, ast.Constant)
            and isinstance(node.slice.value, str)
        ):
            value = node.value
            if (isinstance(value, ast.Name) and value.id == "b") or (
                isinstance(value, ast.Attribute) and value.attr == "_bindings"
            ):
                keys.add(node.slice.value)
    return keys


def _load_win32_keys() -> set[str]:
    tree = ast.parse(Path(win32_backend.__file__).read_text())
    func = next(
        n for n in ast.walk(tree) if isinstance(n, ast.FunctionDef) and n.name == "_load_win32"
    )
    ret = next(n for n in ast.walk(func) if isinstance(n, ast.Return))
    assert isinstance(ret.value, ast.Dict)
    keys = {k.value for k in ret.value.keys if isinstance(k, ast.Constant)}
    # ``**_WIN32_CONSTANTS`` appears as a None key.
    assert any(k is None for k in ret.value.keys)
    return keys | set(win32_backend._WIN32_CONSTANTS)


def test_presenter_binding_keys_exist_in_load_win32() -> None:
    used = _binding_keys_used((_CAPTURE / "presenter.py").read_text())
    used.discard("SetWindowDisplayAffinity")
    assert used
    assert used <= _load_win32_keys()


def test_hotkeys_use_ctrl_alt_prefix() -> None:
    assert win32_backend._MOD_CONTROL | win32_backend._MOD_ALT == 0x3


def _polygon(x0: int, y0: int, x1: int, y1: int, name: str) -> InstanceMask:
    return InstanceMask(
        frame_index=0,
        polygon=((x0, y0), (x1, y0), (x1, y1), (x0, y1)),
        confidence=0.9,
        class_id=0,
        class_name=name,
    )


def test_alpha_black_outer_stroke_is_opaque() -> None:
    style = OutlineStyle(outer_color="#000000", inner_color="#FFFFFF", fill_opacity=0.0)
    renderer = AlphaRenderer(style, alpha_config=AlphaRendererConfig(output_mode="alpha"))
    frame = np.zeros((120, 160, 3), dtype=np.uint8)
    out = renderer.render(frame, [_polygon(40, 30, 120, 90, "x")], frame_index=0)
    black_stroke = (out[:, :, :3] == 0).all(axis=2) & (out[:, :, 3] > 0)
    assert black_stroke.sum() > 20


def test_alpha_resolves_catalog_style_per_class() -> None:
    a = OutlineStyle(inner_color="#F6FF00", outer_color="#000000", fill_opacity=0.0)
    b = OutlineStyle(
        inner_color="#00FFFF", outer_color="#000000", fill_opacity=0.0, stroke_pattern="dashed"
    )
    catalog = TreatmentCatalog(
        default=RoleTreatment(a),
        by_class_name={"aa": RoleTreatment(a), "bb": RoleTreatment(b)},
    )
    renderer = AlphaRenderer(a, catalog=catalog, alpha_config=AlphaRendererConfig())
    frame = np.zeros((120, 320, 3), dtype=np.uint8)
    preds = [_polygon(20, 20, 140, 100, "aa"), _polygon(180, 20, 300, 100, "bb")]
    out = renderer.render(frame, preds, frame_index=0)
    rgb = out[:, :, :3].reshape(-1, 3)
    assert ((rgb == (246, 255, 0)).all(axis=1)).any()
    assert ((rgb == (0, 255, 255)).all(axis=1)).any()


def _fixed(points: list[tuple[int, int]]) -> np.ndarray:
    return (np.array(points, dtype=np.int32) * DRAW_COORDINATE_SCALE).reshape(-1, 1, 2)


def _full_frame_distance(
    frame: np.ndarray, fixed: np.ndarray, outer_w: int, inner_w: int
) -> np.ndarray:
    out = frame.copy()
    mask = np.zeros(frame.shape[:2], dtype=np.uint8)
    cv2.fillPoly(mask, [fixed], 255, shift=4)
    edge = np.where(
        mask > 0,
        cv2.distanceTransform(mask, cv2.DIST_L2, 5),
        cv2.distanceTransform(cv2.bitwise_not(mask), cv2.DIST_L2, 5),
    )
    out[edge <= max(0.5, outer_w / 2.0)] = (0, 0, 0)
    out[edge <= max(0.5, inner_w / 2.0)] = (255, 255, 255)
    return out


def test_distance_roi_matches_full_frame_including_clipped_edges() -> None:
    frame = np.full((60, 80, 3), 90, dtype=np.uint8)
    for pts in ([(20, 15), (50, 18), (40, 40)], [(0, 0), (30, 2), (10, 25)]):
        fixed = _fixed(pts)
        got = frame.copy()
        _draw_distance_outline(cv2, got, fixed, (0, 0, 0), (255, 255, 255), 7, 3)
        np.testing.assert_array_equal(got, _full_frame_distance(frame, fixed, 7, 3))


def test_jfa_roi_only_touches_neighbourhood_and_matches_distance_band() -> None:
    frame = np.zeros((60, 80, 3), dtype=np.uint8)
    fixed = _fixed([(30, 20), (50, 22), (40, 40)])
    jfa, dist = frame.copy(), frame.copy()
    _draw_jfa_outline(cv2, jfa, fixed, (0, 0, 255), (255, 255, 255), 5, 3)
    _draw_distance_outline(cv2, dist, fixed, (0, 0, 255), (255, 255, 255), 5, 3)
    assert jfa[:5].sum() == 0 and jfa[:, :20].sum() == 0
    assert (jfa != dist).any(axis=2).sum() <= 0.05 * (dist != 0).any(axis=2).sum()


def test_fills_roi_matches_full_frame_reference() -> None:
    rng = np.random.default_rng(1)
    frame = rng.integers(0, 255, (60, 80, 3), dtype=np.uint8)
    fixed = _fixed([(20, 15), (50, 18), (40, 40)])
    style = OutlineStyle(fill_opacity=0.2, halo_blur=9)

    tint = apply_tint_fill(cv2, frame, [fixed], style)
    mask = np.zeros(frame.shape[:2], dtype=np.uint8)
    cv2.fillPoly(mask, [fixed], 255, shift=4)
    overlay = frame.copy()
    cv2.fillPoly(overlay, [fixed], (0, 255, 246), shift=4)
    expected = cv2.addWeighted(overlay, 0.2, frame, 0.8, 0)
    np.testing.assert_array_equal(tint, expected)

    halo = apply_halo_fill(cv2, frame, [fixed], style)
    k = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (9, 9))
    alpha = cv2.GaussianBlur(cv2.dilate(mask, k), (9, 9), 0).astype(np.float32) / 255.0 * 0.2
    color = np.array((16, 16, 16), dtype=np.float32)
    ref = frame.astype(np.float32) * (1 - alpha[..., None]) + color * alpha[..., None]
    np.testing.assert_array_equal(halo, np.clip(ref, 0, 255).astype(np.uint8))


def test_tint_roi_keeps_edge_pixels_for_fractional_vertices() -> None:
    frame = np.full((60, 80, 3), 100, dtype=np.uint8)
    pts = np.array([(20.6, 15.6), (50.7, 18.2), (40.6, 40.7)], dtype=np.float64)
    fixed = np.rint(pts * 16).astype(np.int32).reshape(-1, 1, 2)
    style = OutlineStyle(fill_opacity=0.2, halo_blur=9)

    tint = apply_tint_fill(cv2, frame, [fixed], style)
    overlay = frame.copy()
    cv2.fillPoly(overlay, [fixed], (0, 255, 246), shift=4)
    expected = cv2.addWeighted(overlay, 0.2, frame, 0.8, 0)
    np.testing.assert_array_equal(tint, expected)
