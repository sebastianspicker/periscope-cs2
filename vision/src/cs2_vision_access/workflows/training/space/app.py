"""Gradio app for CS2 YOLO11n-seg fine-tuning on Hugging Face Spaces.

Upload a dataset zip (from ``python -m cs2_vision_access.workflows.training.bundle``),
configure hyperparameters, and train. Download FP32 + FP16 ONNX models and
the model manifest when training finishes.

Usage on HF Spaces:
  1. Deploy the package layout documented in this directory's README
  2. Prefer the official HF GPU image so PyTorch/CUDA are preinstalled
  3. Configure the two required Space authentication secrets
  4. Upload your dataset zip and click Start Training
  5. Download the resulting ONNX models + manifest

Local smoke (UI only)::

    python app.py
"""

from __future__ import annotations

import hashlib
import logging
import os  # noqa: F401  # retained for direct Space security tests
import shutil
import sys
import tempfile
import threading
from collections.abc import Callable
from pathlib import Path
from typing import Any

import gradio as gr  # type: ignore[import-not-found]

from cs2_vision_access.workflows.training.space import _gpu_security, _request_security

LOGGER = logging.getLogger(__name__)

# Server-side admission limits. Gradio component bounds are client-side hints;
# callers can invoke ``_train`` directly with arbitrary values.
MAX_UPLOAD_BYTES = 512 * 1024 * 1024
MAX_ZIP_MEMBERS = 10_000
MAX_ZIP_MEMBER_UNCOMPRESSED_BYTES = 128 * 1024 * 1024
MAX_ZIP_TOTAL_UNCOMPRESSED_BYTES = 512 * 1024 * 1024
MAX_ZIP_COMPRESSION_RATIO = 100.0
BASE_MODELS = ("yolo11n-seg.pt", "yolo11s-seg.pt", "yolo11m-seg.pt")
IMAGE_SIZES = (416, 512, 640, 768)

# A GPU training job is deliberately serial. This prevents separate Gradio
# requests from competing for GPU memory or racing any helper-side artifacts.
_TRAINING_SLOT = threading.BoundedSemaphore(value=1)

_TRUSTED_NVIDIA_SMI_ENV = _gpu_security.TRUSTED_NVIDIA_SMI_ENV
_PLATFORM_NVIDIA_SMI_PATHS = _gpu_security.PLATFORM_NVIDIA_SMI_PATHS

# ---------------------------------------------------------------------------
# Shared cloud helpers (package / sibling / vendored cloud only — no
# second full YOLO train pipeline is maintained in this file)
# ---------------------------------------------------------------------------

_ExtractFn = Callable[[str | Path, str | Path], Path]
_TrainFn = Callable[..., Path]
_ManifestFn = Callable[..., Path]

_CLOUD_REQUIRED_MSG = (
    "Training helpers unavailable: could not import "
    "cs2_vision_access.workflows.training.cloud or load cloud package/module. "
    "Vendor training/cloud/ next to app.py (or as ../cloud), or install "
    "the cs2_vision_access package."
)


def _helpers_from_module(mod: object) -> tuple[_ExtractFn, _TrainFn, _ManifestFn] | None:
    """Return extract/train/manifest callables if *mod* exposes them."""
    extract = getattr(mod, "extract_dataset", None)
    train_fn = getattr(mod, "train", None)
    manifest = getattr(mod, "create_manifest", None)
    if callable(extract) and callable(train_fn) and callable(manifest):
        return extract, train_fn, manifest
    return None


def _load_cloud_package_dir(cloud_dir: Path) -> tuple[_ExtractFn, _TrainFn, _ManifestFn] | None:
    """Load a vendored ``cloud/`` package by placing its parent on ``sys.path``.

    Expects ``cloud_dir.name == "cloud"`` so ``import cloud`` resolves the package
    (or a unique alias if ``cloud`` is already taken).
    """
    init_py = cloud_dir / "__init__.py"
    if not init_py.is_file() or cloud_dir.name != "cloud":
        return None
    parent = str(cloud_dir.parent.resolve())
    inserted = parent not in sys.path
    if inserted:
        sys.path.insert(0, parent)
    try:
        # Avoid clobbering an already-imported real package named cloud.
        import importlib

        if "cloud" in sys.modules and getattr(sys.modules["cloud"], "__file__", None):
            existing = Path(str(sys.modules["cloud"].__file__)).resolve()
            if existing.parent == cloud_dir.resolve() or existing == init_py.resolve():
                return _helpers_from_module(sys.modules["cloud"])
        # Fresh import of sibling/vendored cloud package.
        for key in list(sys.modules):
            if key == "cloud" or key.startswith("cloud."):
                # Only clear if it points at a different path.
                mod = sys.modules[key]
                mod_file = getattr(mod, "__file__", None)
                if mod_file is None:
                    continue
                try:
                    if (
                        cloud_dir.resolve() not in Path(mod_file).resolve().parents
                        and Path(mod_file).resolve() != init_py.resolve()
                    ):
                        continue
                except OSError:
                    continue
        importlib.invalidate_caches()
        mod = importlib.import_module("cloud")
        # Confirm we loaded the intended directory.
        mod_file = getattr(mod, "__file__", None)
        if mod_file is None:
            return None
        if Path(mod_file).resolve().parent != cloud_dir.resolve():
            return None
        return _helpers_from_module(mod)
    except (ImportError, OSError) as error:
        LOGGER.debug("Could not load vendored cloud package %s: %s", cloud_dir, error)
        return None
    finally:
        if inserted and sys.path and sys.path[0] == parent:
            sys.path.pop(0)


def _load_cloud_helpers() -> tuple[_ExtractFn | None, _TrainFn | None, _ManifestFn | None]:
    """Load extract_dataset / train / create_manifest from cloud when possible."""
    try:
        from cs2_vision_access.workflows.training.cloud import (
            create_manifest,
            extract_dataset,
            train,
        )

        return extract_dataset, train, create_manifest
    except ImportError:
        pass

    # Monorepo layout: training/cloud/ package next to training/space/.
    # Standalone Space: vendor the complete cloud/ package next to app.py.
    space_dir = Path(__file__).resolve().parent
    for cloud_dir in (space_dir.parent / "cloud", space_dir / "cloud"):
        if cloud_dir.is_dir():
            loaded = _load_cloud_package_dir(cloud_dir)
            if loaded is not None:
                return loaded

    return None, None, None


_cloud_extract, _cloud_train, _cloud_create_manifest = _load_cloud_helpers()


def _canonicalize_space_dataset(data_dir: str | Path) -> Path:
    """Create the fixed manifest required by the untrusted Space upload path."""
    return _request_security.canonicalize_space_dataset(data_dir)


def _confined_dataset_root(data_dir: str | Path, extract_dir: Path) -> Path:
    """Return a real dataset root confined to the private extraction directory."""
    return _request_security.confined_dataset_root(data_dir, extract_dir)


def extract_dataset(zip_path: str | Path, output_dir: str | Path = "cs2_data") -> Path:
    if _cloud_extract is None:
        raise RuntimeError(_CLOUD_REQUIRED_MSG)
    return _cloud_extract(zip_path, output_dir)


def train_model(*args: Any, **kwargs: Any) -> Path:
    if _cloud_train is None:
        raise RuntimeError(_CLOUD_REQUIRED_MSG)
    return _cloud_train(*args, **kwargs)


def create_manifest(onnx_path: str | Path, data_dir: str | Path, **kwargs: Any) -> Path:
    if _cloud_create_manifest is None:
        raise RuntimeError(_CLOUD_REQUIRED_MSG)
    try:
        return _cloud_create_manifest(onnx_path, data_dir, **kwargs)
    except TypeError:
        return _cloud_create_manifest(onnx_path, data_dir)


# ---------------------------------------------------------------------------
# GPU / upload helpers
# ---------------------------------------------------------------------------


def _validated_nvidia_smi_path(candidate: Path) -> str | None:
    """Return a safe executable path, or ``None`` for an untrusted candidate."""
    return _gpu_security.validated_nvidia_smi_path(candidate)


def _nvidia_smi_path() -> str | None:
    """Return a validated platform path or trusted deployment-configured path.

    PATH is intentionally never consulted: an upload request must not be able
    to affect which executable this process invokes.
    """
    return _gpu_security.nvidia_smi_path(
        os.environ.get(_TRUSTED_NVIDIA_SMI_ENV),
        _PLATFORM_NVIDIA_SMI_PATHS,
    )


def _has_cuda() -> bool:
    """Check whether a CUDA-capable GPU is available."""
    return _gpu_security.has_cuda(_nvidia_smi_path, LOGGER)


def _gpu_info() -> str:
    """Return a short GPU description or 'None (CPU)'."""
    return _gpu_security.gpu_info(_nvidia_smi_path, LOGGER)


def _resolve_upload_path(dataset_zip: Any) -> str | None:
    """Normalize Gradio File value to a filesystem path.

    Gradio may pass a path string, a Path-like, or an object with ``.name``
    (e.g. tempfile / NamedString).
    """
    return _request_security.resolve_upload_path(dataset_zip)


def _validate_uploaded_zip(zip_path: Path) -> str | None:
    """Return a user-safe error when the upload exceeds Space admission limits."""
    return _request_security.validate_uploaded_zip(
        zip_path,
        max_upload_bytes=MAX_UPLOAD_BYTES,
        max_members=MAX_ZIP_MEMBERS,
        max_member_bytes=MAX_ZIP_MEMBER_UNCOMPRESSED_BYTES,
        max_total_bytes=MAX_ZIP_TOTAL_UNCOMPRESSED_BYTES,
        max_compression_ratio=MAX_ZIP_COMPRESSION_RATIO,
    )


def _integer_in_range(value: Any, minimum: int, maximum: int) -> int | None:
    """Coerce an integral finite value only when it is within the server bound."""
    return _request_security.integer_in_range(value, minimum, maximum)


def _validate_training_controls(
    base_model: Any,
    epochs: Any,
    batch: Any,
    imgsz: Any,
    lr0: Any,
    patience: Any,
) -> tuple[str, int, int, int, float, int] | str:
    """Validate direct requests independently of Gradio's client-side widgets."""
    return _request_security.validate_training_controls(
        base_model,
        epochs,
        batch,
        imgsz,
        lr0,
        patience,
        base_models=BASE_MODELS,
        image_sizes=IMAGE_SIZES,
    )


def _empty_outputs(message: str) -> tuple[str, None, None, None, str]:
    return message, None, None, None, ""


# ---------------------------------------------------------------------------
# Training entry point
# ---------------------------------------------------------------------------


def _train_job(
    dataset_zip: Any,
    base_model: str,
    epochs: int,
    batch: int,
    imgsz: int,
    lr0: float,
    patience: int,
    progress: gr.Progress = gr.Progress(),
) -> tuple[str, str | None, str | None, str | None, str]:
    """Run fine-tuning and return status + downloadable artifact paths."""
    zip_path = _resolve_upload_path(dataset_zip)
    if not zip_path:
        return _empty_outputs("Error: No dataset uploaded. Please upload a .zip file.")
    if not Path(zip_path).is_file():
        return _empty_outputs(f"Error: Uploaded file not found: {zip_path}")

    has_gpu = _has_cuda()
    device = "cuda:0" if has_gpu else "cpu"
    gpu_name = _gpu_info()

    extract_dir = Path(tempfile.mkdtemp(prefix="cs2_train_"))
    output_dir = Path(tempfile.mkdtemp(prefix="cs2_train_artifacts_"))
    completed = False

    try:
        progress(0.05, desc="Extracting dataset...")
        try:
            data_dir = extract_dataset(zip_path, extract_dir)
            data_dir = _confined_dataset_root(data_dir, extract_dir)
        except Exception as exc:
            return _empty_outputs(f"Error extracting zip: {exc}")

        images_dir = data_dir / "images"
        labels_dir = data_dir / "labels"
        if not images_dir.is_dir() or not labels_dir.is_dir():
            return _empty_outputs(
                "Error: zip must contain images/ and labels/ directories "
                "(create it with: python -m cs2_vision_access.workflows.training.bundle)."
            )

        label_files = list(labels_dir.glob("*.txt"))
        if not label_files:
            return _empty_outputs("Error: no .txt label files found under labels/.")

        try:
            _canonicalize_space_dataset(data_dir)
        except (OSError, ValueError) as exc:
            return _empty_outputs(f"Error validating dataset metadata: {exc}")

        progress(0.1, desc=f"Found {len(label_files)} labeled frames")
        progress(0.15, desc=f"Loading model on {device} ({gpu_name})...")

        try:
            progress(0.2, desc=f"Training on {device}...")
            onnx_path = train_model(
                data_dir,
                base_model=base_model,
                epochs=int(epochs),
                batch=int(batch),
                imgsz=int(imgsz),
                lr0=float(lr0),
                patience=int(patience),
                device=device,
            )
        except Exception as exc:
            return _empty_outputs(f"Error during training/export: {exc}")

        onnx_path = Path(onnx_path)
        if not onnx_path.is_file():
            return _empty_outputs("Error: training did not produce an ONNX model.")

        progress(0.90, desc="Creating manifest...")
        origin = f"Fine-tuned on Hugging Face Spaces ({gpu_name})"
        try:
            manifest_path = create_manifest(onnx_path, data_dir, origin=origin)
        except Exception as exc:
            return _empty_outputs(f"Error creating manifest: {exc}")

        sha256 = hashlib.sha256(onnx_path.read_bytes()).hexdigest()
        onnx_size_mb = onnx_path.stat().st_size / 1e6

        job_id = output_dir.name.removeprefix("cs2_train_artifacts_")
        final_onnx = output_dir / f"{job_id}-cs2-yolo11n-seg.onnx"
        final_manifest = output_dir / f"{job_id}-cs2-yolo11n-seg.model.json"
        shutil.copy2(str(onnx_path), str(final_onnx))
        shutil.copy2(str(manifest_path), str(final_manifest))

        fp16_src = data_dir / "cs2-yolo11n-seg-fp16.onnx"
        final_fp16: Path | None = None
        fp16_str = ""
        if fp16_src.is_file():
            final_fp16 = output_dir / f"{job_id}-cs2-yolo11n-seg-fp16.onnx"
            shutil.copy2(str(fp16_src), str(final_fp16))
            fp16_str = f"\nFP16 ONNX: {final_fp16.stat().st_size / 1e6:.1f} MB (GPU inference)"

        device_str = f"GPU ({gpu_name})" if has_gpu else "CPU"
        backend = "cloud helpers" if _cloud_train is not None else "unavailable"
        msg = (
            f"Training complete! {len(label_files)} frames, {int(epochs)} epochs\n"
            f"Device: {device_str}\n"
            f"Backend: {backend}\n"
            f"FP32 ONNX: {onnx_size_mb:.1f} MB (CPU inference){fp16_str}\n"
            f"SHA-256: {sha256[:16]}..."
        )
        progress(1.0, desc="Done")
        completed = True
        return (
            msg,
            str(final_onnx),
            str(final_fp16) if final_fp16 is not None else None,
            str(final_manifest),
            sha256,
        )
    finally:
        shutil.rmtree(extract_dir, ignore_errors=True)
        if not completed:
            shutil.rmtree(output_dir, ignore_errors=True)


def _train(
    dataset_zip: Any,
    base_model: str,
    epochs: int,
    batch: int,
    imgsz: int,
    lr0: float,
    patience: int,
    progress: gr.Progress = gr.Progress(),
) -> tuple[str, str | None, str | None, str | None, str]:
    """Validate a request and run one isolated training job if capacity permits."""
    zip_path = _resolve_upload_path(dataset_zip)
    if not zip_path:
        return _empty_outputs("Error: No dataset uploaded. Please upload a .zip file.")
    upload_path = Path(zip_path)
    if not upload_path.is_file():
        return _empty_outputs("Error: uploaded file is unavailable.")
    upload_error = _validate_uploaded_zip(upload_path)
    if upload_error is not None:
        return _empty_outputs(upload_error)

    controls = _validate_training_controls(base_model, epochs, batch, imgsz, lr0, patience)
    if isinstance(controls, str):
        return _empty_outputs(controls)

    try:
        acquired = _TRAINING_SLOT.acquire(blocking=False)
    except Exception:
        LOGGER.exception("Training capacity gate failed closed")
        return _empty_outputs("Error: training capacity is temporarily unavailable.")
    if not acquired:
        return _empty_outputs(
            "Error: another training job is already running. Please try again later."
        )

    try:
        (
            checked_model,
            checked_epochs,
            checked_batch,
            checked_imgsz,
            checked_lr0,
            checked_patience,
        ) = controls
        return _train_job(
            upload_path,
            checked_model,
            checked_epochs,
            checked_batch,
            checked_imgsz,
            checked_lr0,
            checked_patience,
            progress,
        )
    finally:
        _TRAINING_SLOT.release()


# ---------------------------------------------------------------------------
# Gradio UI
# ---------------------------------------------------------------------------


def _build_ui() -> gr.Blocks:
    gpu_name = _gpu_info()
    has_gpu = _has_cuda()
    gpu_badge = (
        f"**GPU:** `{gpu_name}` — ready for training"
        if has_gpu
        else f"**GPU:** `{gpu_name}` — CPU only (slow; use a T4 Space for real training)"
    )
    helpers_ok = _cloud_train is not None
    helpers = (
        "shared `cloud` helpers"
        if helpers_ok
        else "unavailable — vendor `cloud/` next to app.py or install package"
    )
    helpers_hint = (
        ""
        if helpers_ok
        else (
            "\n\n**Setup:** Training will not run until helpers load. "
            "Copy `training/cloud/` next to `app.py` (standalone Space), "
            "or install `cs2_vision_access` / keep monorepo sibling `../cloud/`."
        )
    )

    with gr.Blocks(title="CS2 YOLO11n-seg Training", theme=gr.themes.Soft()) as demo:
        gr.Markdown(
            "# CS2 YOLO11n-seg Fine-Tuning\n"
            "Upload a dataset zip from "
            "`python -m cs2_vision_access.workflows.training.bundle`, configure training, "
            "and download the fine-tuned ONNX models (FP32 + FP16) plus manifest.\n\n"
            f"{gpu_badge}\n\n"
            f"_Training backend: {helpers}_{helpers_hint}\n\n"
            "**Deployment security:** launch requires the server-configured Gradio "
            "credentials. Deploy it only as a private, access-controlled Space."
        )

        with gr.Row():
            with gr.Column(scale=1):
                dataset_zip = gr.File(
                    label="Dataset ZIP",
                    file_types=[".zip"],
                    file_count="single",
                )

                base_model = gr.Dropdown(
                    label="Base model",
                    choices=list(BASE_MODELS),
                    value="yolo11n-seg.pt",
                )

                epochs = gr.Slider(minimum=10, maximum=500, value=150, step=10, label="Epochs")
                batch = gr.Slider(minimum=2, maximum=64, value=16, step=2, label="Batch size")
                imgsz = gr.Dropdown(label="Image size", choices=list(IMAGE_SIZES), value=416)
                lr0 = gr.Number(
                    label="Learning rate",
                    value=0.001,
                    minimum=0.0001,
                    maximum=0.01,
                    step=0.0005,
                )
                patience = gr.Slider(
                    minimum=10,
                    maximum=200,
                    value=50,
                    step=10,
                    label="Early stop patience",
                )

                train_btn = gr.Button("Start Training", variant="primary", size="lg")

            with gr.Column(scale=1):
                status = gr.Textbox(label="Status", lines=8, interactive=False)
                onnx_file = gr.File(label="Download FP32 ONNX (CPU inference)")
                fp16_file = gr.File(label="Download FP16 ONNX (GPU inference)")
                manifest_file = gr.File(label="Download manifest JSON")
                sha_display = gr.Textbox(label="SHA-256 (FP32)", lines=1, interactive=False)

        train_btn.click(
            fn=_train,
            inputs=[dataset_zip, base_model, epochs, batch, imgsz, lr0, patience],
            outputs=[status, onnx_file, fp16_file, manifest_file, sha_display],
        )

        gr.Markdown(
            "### Instructions\n"
            "1. **Prepare dataset** (locally):\n"
            "   - Build labels/images (e.g. `python -m "
            "cs2_vision_access.workflows.training.prepare`)\n"
            "   - Bundle for upload:\n"
            "     `python -m cs2_vision_access.workflows.training.bundle "
            "--input data/cs2_train --output data/cs2_train_bundle.zip`\n"
            "   - Optional: `--max-frames N` to cap frames for a quicker Space run\n"
            "2. **Upload** the zip above (must contain `images/` and `labels/`; "
            "its `dataset.yaml`, if present, is replaced with the Space's fixed "
            "local manifest).\n"
            "3. **Configure** epochs, batch size, image size, etc.\n"
            "4. **Train** — progress updates in Status.\n"
            "5. **Download** FP32 ONNX (CPU), FP16 ONNX (GPU), and the manifest.\n\n"
            "**Hardware:** Prefer a **T4 GPU** Space (~$0.40/hr) or better. "
            "About 150 epochs on ~500 frames is roughly 30–60 minutes on a T4. "
            "CPU Spaces will work for UI smoke tests but are impractical for full training.\n\n"
            "**Note:** Torch with CUDA is usually provided by the Hugging Face GPU "
            "runtime image; this Space's `requirements.txt` does not pin a CUDA torch wheel."
        )

    return demo


def _required_space_auth() -> tuple[str, str]:
    """Return required private-Space credentials without exposing their values."""
    return _request_security.required_space_auth(os.environ)


def main() -> None:
    """Launch the Space only after its private-access credentials are configured."""
    auth = _required_space_auth()
    demo = _build_ui()
    demo.launch(
        auth=auth,
        max_file_size=MAX_UPLOAD_BYTES,
        show_error=False,
        enable_monitoring=False,
    )


if __name__ == "__main__":
    main()
