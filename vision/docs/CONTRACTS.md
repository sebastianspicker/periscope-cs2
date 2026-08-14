# Runtime contracts

Stable interfaces for the outline pipeline. Implementations may change; these shapes should not without a deliberate migration.

## Scope

- Offline mode: local video in, optional local MP4 or windowed preview out.
- Live mode: screen, capture-device, or file input; per-frame inference; overlay or alpha/green composite output.
- One segmenter backend at a time via `create_segmenter`.
- Checksum-bound ONNX weights via JSON manifest (where the backend requires one).
- No process injection, game-memory access, or synthetic input in this package.

## Segmenter

`cs2_vision_access.segmenters.protocol.Segmenter`:

```python
class Segmenter(Protocol):
    def predict(
        self, frame_bgr: np.ndarray, *, frame_index: int
    ) -> tuple[InstanceMask, ...]: ...
```

| Rule | Detail |
|------|--------|
| Current frame only | Describe `frame_bgr` for the given `frame_index` |
| BGR layout | Shape `(height, width, 3)` as OpenCV uses |
| Non-negative index | `frame_index >= 0` |
| Empty is valid | No detections returns `()` |
| Fail closed | Malformed backend output raises rather than inventing geometry |

Registered backend names (see `segmenters.protocol.SUPPORTED_SEGMENTER_BACKENDS`):

| Name | Role |
|------|------|
| `ultralytics-onnx` | Default YOLO-seg ONNX |
| `ultralytics-detect` | Box rectangles only |
| `rfdetr` | Optional RF-DETR |
| `cs2-sam` | Vombit + EdgeSAM hybrid |
| `yolov10`, `nanodet` | Detect-only ORT |

Aliases (e.g. `yolo-detect` → `ultralytics-detect`, `rf-detr` → `rfdetr`) are normalized in `normalize_segmenter_backend`.

## InstanceMask

Defined in `cs2_vision_access.predictions`. Polygons are pixel coordinates for the current frame. Renderer and evaluation code consume this type.

## Model manifest

`model_manifest` / training export writers attach SHA-256 and metadata so loaders can refuse mismatched weights. Prefer always pairing `--model` with `--manifest` in CLI tools that require both.

## Dataset layouts

- Flat: `images/` + `labels/` + `dataset.yaml` (research bootstrap; train=val is leaky).
- Session split: `images/{train,val,test}/` with matching labels; whole `session_id` per split.

Class maps for product single-class work use id `0` = `player` (`training.contracts.PRODUCT_CLASSES`). Vombit multi-class maps remain available for detector-side labeling.

## External-only operation

Capture APIs read pixels from the OS or a capture card. Overlay APIs draw a separate window. See `safety.py` and package comments for the intended boundary.

## Configuration

`cs2-vision-config.json` persists an `AppConfig` (schema version 1) with `input`, `model`, `outline`, and `display` sections. The `display` section carries overlay placement: `overlay_x`, `overlay_y` (window top-left in pixels) and `overlay_monitor` (`0` = auto). The desktop GUI (`cs2-vision gui`) reads and writes the same config file and drives the same `run_live_pipeline` engine as the CLI.
