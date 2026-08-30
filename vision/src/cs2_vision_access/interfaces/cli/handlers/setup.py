"""setup subcommand — one-command setup for the live ingame overlay.

Checks dependencies, downloads a model, creates a configuration file, and
optionally tests the pipeline. Designed to get a visually impaired user from
zero to running in a single command after the locked environment is installed.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import TYPE_CHECKING, Any

from cs2_vision_access.adapters.capture.base import list_capture_devices
from cs2_vision_access.adapters.capture.screen import list_monitors
from cs2_vision_access.application.configuration import (
    AppConfig,
    DisplayConfig,
    InputConfig,
    ModelConfig,
    OutlineConfig,
    save_config,
)
from cs2_vision_access.interfaces.cli.handlers._model_ops import create_manifest, export_onnx
from cs2_vision_access.interfaces.cli.types import Subcommands

if TYPE_CHECKING:
    import numpy as np


_TRAINING_ENVIRONMENT_COMMAND = "uv sync --frozen --extra train"


def register_setup_command(
    subcommands: Subcommands,
) -> None:
    setup = subcommands.add_parser(
        "setup",
        help="one-command setup: check deps, download model, create config",
        description=(
            "Guides you through setting up the live ingame overlay pipeline. "
            "Checks Python and dependencies, downloads an ONNX model, creates "
            "a configuration file, and optionally tests the pipeline."
        ),
    )
    setup.add_argument(
        "--auto",
        action="store_true",
        help="Run in fully automatic mode with defaults (no prompts)",
    )
    setup.add_argument(
        "--model",
        default="yolo11n-seg",
        help="Model name to download (default: yolo11n-seg)",
    )
    setup.add_argument(
        "--output-dir",
        type=Path,
        default=Path("artifacts"),
        help="Directory for model and manifest (default: artifacts/)",
    )
    setup.add_argument(
        "--config",
        type=Path,
        default=None,
        help="Path to save the configuration file (default: ./cs2-vision-config.json)",
    )
    setup.add_argument(
        "--input-type",
        choices=["screen", "capture-device", "file"],
        default="screen",
        help="Default input source type (default: screen)",
    )
    setup.add_argument(
        "--device",
        default="cpu",
        help="Device for inference: cpu or cuda:0 (default: cpu)",
    )
    setup.add_argument(
        "--skip-download",
        action="store_true",
        help="Skip model download (use existing files)",
    )
    setup.add_argument(
        "--skip-test",
        action="store_true",
        help="Skip the pipeline test at the end",
    )
    setup.add_argument(
        "--overlay",
        action="store_true",
        default=None,
        help="Enable transparent always-on-top overlay window",
    )
    setup.add_argument(
        "--output-mode",
        choices=["overlay", "alpha", "green"],
        default=None,
        help="Compositing output mode (default: overlay, auto-switches to alpha with --overlay)",
    )
    setup.set_defaults(handler=_handle_setup)


# ------------------------------------------------------------------
# Step helpers (extracted from _handle_setup for readability)
# ------------------------------------------------------------------


def _check_environment() -> list[str]:
    """Check Python version and key dependency availability.

    Returns list of missing package names (empty = all good).
    """
    print("[1/6] Checking Python version and environment...")
    py_version = sys.version_info
    if py_version < (3, 11):
        print(f"  ERROR: Python 3.11+ required, found {py_version.major}.{py_version.minor}")
        sys.exit(1)

    missing: list[str] = []
    for dep_name, import_name in [
        ("numpy", "numpy"),
        ("opencv-python", "cv2"),
        ("PyYAML", "yaml"),
        ("mss", "mss"),
        ("onnxruntime", "onnxruntime"),
        ("ultralytics", "ultralytics"),
    ]:
        try:
            __import__(import_name)
        except ImportError:
            missing.append(dep_name)

    if missing:
        print(f"  WARNING: missing packages: {', '.join(missing)}")
        print("  Install the locked environment before continuing:")
        print(f"    {_TRAINING_ENVIRONMENT_COMMAND}")
    else:
        print("  All core dependencies available")
    print()
    return missing


def _prepare_output_dir(arguments: argparse.Namespace) -> Path:
    """Create and return the resolved output directory."""
    print("[2/6] Preparing output directory...")
    output_dir = Path(arguments.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    print(f"  Output directory: {output_dir}")
    print()
    return output_dir


def _download_and_export_model(
    arguments: argparse.Namespace,
    output_dir: Path,
) -> tuple[Path, Path]:
    """Download, export to ONNX, and create manifest.

    Returns ``(onnx_path, manifest_path)``.
    """
    print("[3/6] Setting up model...")
    model_name = arguments.model
    onnx_path = output_dir / f"{model_name}.onnx"
    manifest_path = output_dir / f"{model_name}.model.json"

    if arguments.skip_download:
        print("  Skipping download (--skip-download)")
    elif onnx_path.is_file() and manifest_path.is_file():
        print(f"  Model already exists: {onnx_path.name}")
    else:
        print(f"  Downloading and exporting {model_name}...")
        try:
            from cs2_vision_access.interfaces.cli.handlers.download_model import (
                _DOWNLOADABLE_MODELS,
            )

            model_info = _DOWNLOADABLE_MODELS.get(model_name)
            if model_info is None:
                print(f"  Unknown model {model_name!r}, trying export from local .pt...")

            try:
                import ultralytics
            except ImportError as error:
                message = (
                    "Ultralytics is required to download and export a setup model. "
                    "Install the locked training environment before rerunning: "
                    f"{_TRAINING_ENVIRONMENT_COMMAND}"
                )
                print(f"  ERROR: {message}")
                raise RuntimeError(message) from error
            YOLO: Any = ultralytics.__dict__["YOLO"]

            pt_path = output_dir / f"{model_name}.pt"
            if not pt_path.is_file():
                model = YOLO(f"{model_name}.pt")
                model.save(str(pt_path))
            else:
                model = YOLO(str(pt_path))

            export_onnx(pt_path, onnx_path, image_size=640, device="cpu")

            origin = (
                model_info.get("default_origin", "https://docs.ultralytics.com")
                if model_info
                else "https://docs.ultralytics.com"
            )
            create_manifest(
                onnx_path,
                manifest_path,
                model_name=model_name,
                model_info={"task": "segment", "default_origin": origin},
                image_size=640,
                origin=origin,
                license_name="AGPL-3.0-only",
                classes=["person"],
            )

            if pt_path.is_file():
                pt_path.unlink()

            print(f"  Model ONNX: {onnx_path.name}")
            print(f"  Manifest:   {manifest_path.name}")
        except Exception as error:
            print(f"  ERROR: Model setup failed: {error}")
            print("  You can try again manually with: cs2-vision download-model")
            raise

    # Fail closed: never claim setup complete with missing model artifacts.
    missing = [str(p) for p in (onnx_path, manifest_path) if not p.is_file()]
    if missing:
        print("  ERROR: Model artifacts missing after setup step:")
        for path in missing:
            print(f"    - {path}")
        if arguments.skip_download:
            print(
                "  Provide existing ONNX + manifest under the output dir, "
                "or re-run without --skip-download."
            )
        else:
            print("  Re-run setup, or use: cs2-vision download-model")
        raise FileNotFoundError(
            "setup requires both ONNX model and manifest; missing: " + ", ".join(missing)
        )

    print(f"  Model ready: {onnx_path.name}")
    print(f"  Manifest ready: {manifest_path.name}")
    print()
    return onnx_path, manifest_path


def _detect_input_sources(arguments: argparse.Namespace) -> InputConfig:
    """Detect available capture devices / monitors and return an InputConfig."""
    print("[4/6] Detecting input sources...")
    capture_devices = list_capture_devices()
    monitors: list[dict[str, object]] = []
    try:
        monitors = list_monitors()
    except Exception as error:
        print(f"  (monitor enumeration unavailable: {error})")

    cfg = InputConfig(source_type=arguments.input_type)

    if capture_devices and not arguments.auto:
        print(f"  Found {len(capture_devices)} capture device(s):")
        for dev in capture_devices:
            print(f"    [{dev.index}] {dev.name}")
    elif capture_devices:
        cfg = InputConfig(
            source_type="capture-device",
            device_index=capture_devices[0].index,
        )
        print(
            f"  Using first capture device: [{capture_devices[0].index}] {capture_devices[0].name}"
        )

    if monitors and not capture_devices:
        print(f"  Found {len(monitors)} monitor(s):")
        for mon in monitors:
            print(
                f"    [{mon['index']}] {mon['width']}x{mon['height']} "
                f"at ({mon['left']},{mon['top']})"
            )
        if arguments.auto:
            first = monitors[0]
            cfg = InputConfig(
                source_type="screen",
                monitor_index=_monitor_dimension(first["index"], "index"),
                width=_monitor_dimension(first["width"], "width"),
                height=_monitor_dimension(first["height"], "height"),
            )
    print()
    return cfg


def _monitor_dimension(value: object, field: str) -> int:
    """Validate the object-valued capture adapter payload before conversion."""
    if isinstance(value, bool) or not isinstance(value, (int, float, str)):
        raise ValueError(f"monitor {field} must be an integer-compatible value")
    return int(value)


def _write_config(
    arguments: argparse.Namespace,
    onnx_path: Path,
    manifest_path: Path,
    input_config: InputConfig,
) -> Path:
    """Create and save the application configuration."""
    print("[5/6] Creating configuration...")
    use_overlay = bool(arguments.overlay) if arguments.overlay is not None else False
    output_mode = arguments.output_mode or ("alpha" if use_overlay else "overlay")

    config = AppConfig(
        input=input_config,
        model=ModelConfig(
            path=str(onnx_path),
            manifest=str(manifest_path),
            class_names=("person",),
            device=arguments.device,
        ),
        outline=OutlineConfig(preset="maximum-visibility", output_mode=output_mode),
        display=DisplayConfig(scale=0.5, headless=False, overlay=use_overlay),
    )
    config_path = save_config(config, path=arguments.config, overwrite=True)
    print(f"  Config saved: {config_path}")
    print()
    return config_path


def _read_capture_frames(cap: Any) -> list[np.ndarray]:
    """Read up to five frames from an OpenCV-compatible capture object."""
    frames: list[np.ndarray] = []
    for _ in range(5):
        ok, frame = cap.read()
        if not ok or frame is None:
            break
        frames.append(frame)
    return frames


def _read_test_frames(input_config: InputConfig) -> list[np.ndarray]:
    """Read up to five frames from the configured source for a sanity check.

    Capture-device and screen sources fall back to a single 640x480 dummy frame
    only when no device is available. A ``file`` source opens the configured
    path with ``cv2.VideoCapture`` and is never substituted.
    """
    import numpy as np

    if input_config.source_type == "screen":
        from cs2_vision_access.adapters.capture.screen import ScreenCapturer

        try:
            capturer = ScreenCapturer(monitor_index=input_config.monitor_index)
            try:
                return [capturer.grab() for _ in range(5)]
            finally:
                capturer.release()
        except Exception as error:
            print(f"  WARNING: screen capture unavailable ({error}); using dummy test frame")
            return [np.zeros((480, 640, 3), dtype=np.uint8)]

    if input_config.source_type == "capture-device":
        from cs2_vision_access.adapters.capture.base import CaptureConfig, open_capture

        try:
            cap = open_capture(
                CaptureConfig(
                    source=input_config.device_name or input_config.device_index,
                    preferred_width=input_config.width,
                    preferred_height=input_config.height,
                )
            )
        except Exception as error:
            print(f"  WARNING: capture device unavailable ({error}); using dummy test frame")
            return [np.zeros((480, 640, 3), dtype=np.uint8)]
        try:
            frames = _read_capture_frames(cap)
        finally:
            cap.release()
        if frames:
            return frames
        print("  WARNING: could not read from capture device; using dummy test frame")
        return [np.zeros((480, 640, 3), dtype=np.uint8)]

    file_path = input_config.file_path
    if file_path:
        import cv2

        cap = cv2.VideoCapture(file_path)
        try:
            if not cap.isOpened():
                raise RuntimeError(f"could not open file source: {file_path}")
            frames = _read_capture_frames(cap)
        finally:
            cap.release()
        if not frames:
            raise RuntimeError(f"could not read any frames from {file_path}")
        return frames

    print("  WARNING: no file path configured for file source; using dummy test frame")
    return [np.zeros((480, 640, 3), dtype=np.uint8)]


def _test_pipeline(
    onnx_path: Path,
    manifest_path: Path,
    input_config: InputConfig,
    arguments: argparse.Namespace,
) -> None:
    """Run a five-frame sanity check through the model."""
    if arguments.skip_test:
        print("[6/6] Skipping pipeline test (--skip-test)")
        return

    print("[6/6] Testing pipeline (5 frames)...")
    try:
        from cs2_vision_access.adapters.models.segmenters import create_segmenter

        segmenter = create_segmenter(
            onnx_path,
            manifest_path,
            class_names=("person",),
            confidence=0.45,
            image_size=640,
            device=arguments.device,
        )

        frames = _read_test_frames(input_config)
        if not frames:
            raise RuntimeError("no frames could be read from the configured source")

        for index, frame in enumerate(frames):
            predictions = segmenter.predict(frame, frame_index=index)
            print(
                f"  Frame {index + 1}: {frame.shape[1]}x{frame.shape[0]}, "
                f"predictions: {len(predictions)}"
            )
        print(f"  Pipeline test OK ({len(frames)} frames)")
    except Exception as error:
        print(f"  WARNING: Pipeline test failed: {error}")
        print("  You can still try running manually.")
    print()


def _print_final_instructions(
    config_path: Path,
    onnx_path: Path,
    manifest_path: Path,
    use_overlay: bool,
) -> None:
    """Print closing instructions for the user."""
    print("=" * 60)
    print(" SETUP COMPLETE")
    print("=" * 60)
    print()
    print("Run the live overlay:")
    print()
    if use_overlay:
        print(f"  cs2-vision live --config {config_path}")
        print()
        print("  (the config already enables --overlay and --output-mode alpha)")
    else:
        print(f"  cs2-vision live --config {config_path}")
        print()
        print("  For transparent overlay add:  --overlay --output-mode alpha")
    print()
    print("Or with full options:")
    print(f"  cs2-vision live --model {onnx_path} \\")
    print(f"    --manifest {manifest_path} \\")
    print("    --class-name person \\")
    print("    --outline-preset maximum-visibility \\")
    print("    --source-type screen \\")
    print("    --output-mode alpha --overlay")
    print()
    print("Hotkeys in the live window:")
    print("  [1-3]   Change outline preset")
    print("  [+/-]   Adjust outline width")
    print("  [T]     Toggle temporal stability")
    print("  [O]     Cycle output mode (overlay/alpha/green)")
    print("  [F]     Toggle fill")
    print("  [Space] Pause/resume")
    print("  [Q]     Quit")
    print()


def _handle_setup(arguments: argparse.Namespace) -> int:
    print("=" * 60)
    print(" CS2 Vision Access — Ingame Overlay Setup")
    print("=" * 60)
    print()

    _check_environment()
    output_dir = _prepare_output_dir(arguments)

    try:
        onnx_path, manifest_path = _download_and_export_model(arguments, output_dir)
    except Exception:
        return 1

    input_config = _detect_input_sources(arguments)
    config_path = _write_config(arguments, onnx_path, manifest_path, input_config)
    _test_pipeline(onnx_path, manifest_path, input_config, arguments)

    use_overlay = bool(arguments.overlay) if arguments.overlay is not None else False
    _print_final_instructions(config_path, onnx_path, manifest_path, use_overlay)
    return 0
