"""Gradio app for CS2 YOLO11n-seg fine-tuning on Hugging Face Spaces.

Upload a dataset zip (from ``python -m cs2_vision_access.training.bundle``),
configure hyperparameters, and train. Download FP32 + FP16 ONNX models and
the model manifest when training finishes.

Usage on HF Spaces:
  1. Create a Space from this directory (GPU recommended: T4 small or better)
  2. Prefer the official HF GPU image so PyTorch/CUDA are preinstalled
  3. Vendor ``cloud/`` (or legacy ``cloud.py``) next to ``app.py`` for train
  4. Upload your dataset zip and click Start Training
  5. Download the resulting ONNX models + manifest

Local smoke (UI only)::

    python app.py
"""

from __future__ import annotations

import hashlib
import importlib.util
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Callable
from pathlib import Path
from typing import Any

import gradio as gr

# ---------------------------------------------------------------------------
# Shared cloud helpers (package / sibling / vendored cloud only — no
# second full YOLO train pipeline is maintained in this file)
# ---------------------------------------------------------------------------

_ExtractFn = Callable[[str | Path, str | Path], Path]
_TrainFn = Callable[..., Path]
_ManifestFn = Callable[..., Path]

_CLOUD_REQUIRED_MSG = (
    "Training helpers unavailable: could not import "
    "cs2_vision_access.training.cloud or load cloud package/module. "
    "Vendor training/cloud/ (or legacy cloud.py) next to app.py "
    "(or as ../cloud), or install the cs2_vision_access package."
)


def _helpers_from_module(mod: object) -> tuple[_ExtractFn, _TrainFn, _ManifestFn] | None:
    """Return extract/train/manifest callables if *mod* exposes them."""
    extract = getattr(mod, "extract_dataset", None)
    train_fn = getattr(mod, "train", None)
    manifest = getattr(mod, "create_manifest", None)
    if callable(extract) and callable(train_fn) and callable(manifest):
        return extract, train_fn, manifest  # type: ignore[return-value]
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
    except Exception:
        return None
    finally:
        if inserted:
            try:
                if sys.path and sys.path[0] == parent:
                    sys.path.pop(0)
            except Exception:
                pass


def _load_cloud_helpers() -> tuple[_ExtractFn | None, _TrainFn | None, _ManifestFn | None]:
    """Load extract_dataset / train / create_manifest from cloud when possible."""
    try:
        from cs2_vision_access.training.cloud import (  # type: ignore[import-not-found]
            create_manifest,
            extract_dataset,
            train,
        )

        return extract_dataset, train, create_manifest
    except ImportError:
        pass

    # Monorepo layout: training/cloud/ package next to training/space/
    # Standalone Space: vendor cloud/ or legacy cloud.py next to app.py
    space_dir = Path(__file__).resolve().parent
    for cloud_dir in (space_dir.parent / "cloud", space_dir / "cloud"):
        if cloud_dir.is_dir():
            loaded = _load_cloud_package_dir(cloud_dir)
            if loaded is not None:
                return loaded

    for cloud_path in (space_dir.parent / "cloud.py", space_dir / "cloud.py"):
        if not cloud_path.is_file():
            continue
        try:
            spec = importlib.util.spec_from_file_location(
                "_cs2_vision_access_training_cloud", cloud_path
            )
            if spec is not None and spec.loader is not None:
                mod = importlib.util.module_from_spec(spec)
                sys.modules[spec.name] = mod
                spec.loader.exec_module(mod)
                helpers = _helpers_from_module(mod)
                if helpers is not None:
                    return helpers
        except Exception:
            continue

    return None, None, None


_cloud_extract, _cloud_train, _cloud_create_manifest = _load_cloud_helpers()


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


def _has_cuda() -> bool:
    """Check whether a CUDA-capable GPU is available."""
    try:
        import torch

        return bool(torch.cuda.is_available())
    except ImportError:
        try:
            return (
                subprocess.run(
                    ["nvidia-smi"], capture_output=True, check=False, timeout=3
                ).returncode
                == 0
            )
        except Exception:
            return False


def _gpu_info() -> str:
    """Return a short GPU description or 'None (CPU)'."""
    try:
        import torch

        if torch.cuda.is_available():
            name = torch.cuda.get_device_name(0)
            vram = torch.cuda.get_device_properties(0).total_memory / 1e9
            return f"{name} ({vram:.1f} GB)"
    except Exception:
        pass
    try:
        result = subprocess.run(
            ["nvidia-smi", "--query-gpu=name,memory.total", "--format=csv,noheader"],
            capture_output=True,
            text=True,
            check=False,
            timeout=3,
        )
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout.strip().split(",")[0].strip()
    except Exception:
        pass
    return "None (CPU)"


def _resolve_upload_path(dataset_zip: Any) -> str | None:
    """Normalize Gradio File value to a filesystem path.

    Gradio may pass a path string, a Path-like, or an object with ``.name``
    (e.g. tempfile / NamedString).
    """
    if dataset_zip is None:
        return None
    if isinstance(dataset_zip, (str, Path)):
        path = str(dataset_zip).strip()
        return path or None
    name = getattr(dataset_zip, "name", None)
    if name:
        path = str(name).strip()
        return path or None
    return None


def _empty_outputs(message: str) -> tuple[str, None, None, None, str]:
    return message, None, None, None, ""


# ---------------------------------------------------------------------------
# Training entry point
# ---------------------------------------------------------------------------


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
    output_dir = Path(tempfile.gettempdir()) / "cs2-output"
    output_dir.mkdir(parents=True, exist_ok=True)

    try:
        progress(0.05, desc="Extracting dataset...")
        try:
            data_dir = extract_dataset(zip_path, extract_dir)
        except Exception as exc:
            return _empty_outputs(f"Error extracting zip: {exc}")

        images_dir = data_dir / "images"
        labels_dir = data_dir / "labels"
        if not images_dir.is_dir() or not labels_dir.is_dir():
            return _empty_outputs(
                "Error: zip must contain images/ and labels/ directories "
                "(create it with: python -m cs2_vision_access.training.bundle)."
            )

        label_files = list(labels_dir.glob("*.txt"))
        if not label_files:
            return _empty_outputs("Error: no .txt label files found under labels/.")

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

        final_onnx = output_dir / "cs2-yolo11n-seg.onnx"
        final_manifest = output_dir / "cs2-yolo11n-seg.model.json"
        shutil.copy2(str(onnx_path), str(final_onnx))
        shutil.copy2(str(manifest_path), str(final_manifest))

        fp16_src = data_dir / "cs2-yolo11n-seg-fp16.onnx"
        final_fp16: Path | None = None
        fp16_str = ""
        if fp16_src.is_file():
            final_fp16 = output_dir / "cs2-yolo11n-seg-fp16.onnx"
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
        return (
            msg,
            str(final_onnx),
            str(final_fp16) if final_fp16 is not None else None,
            str(final_manifest),
            sha256,
        )
    finally:
        shutil.rmtree(extract_dir, ignore_errors=True)


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
            "`python -m cs2_vision_access.training.bundle`, configure training, "
            "and download the fine-tuned ONNX models (FP32 + FP16) plus manifest.\n\n"
            f"{gpu_badge}\n\n"
            f"_Training backend: {helpers}_{helpers_hint}"
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
                    choices=["yolo11n-seg.pt", "yolo11s-seg.pt", "yolo11m-seg.pt"],
                    value="yolo11n-seg.pt",
                )

                epochs = gr.Slider(minimum=10, maximum=500, value=150, step=10, label="Epochs")
                batch = gr.Slider(minimum=2, maximum=64, value=16, step=2, label="Batch size")
                imgsz = gr.Dropdown(label="Image size", choices=[416, 512, 640, 768], value=416)
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
            "   - Build labels/images (e.g. `python -m cs2_vision_access.training.prepare`)\n"
            "   - Bundle for upload:\n"
            "     `python -m cs2_vision_access.training.bundle "
            "--input data/cs2_train --output data/cs2_train_bundle.zip`\n"
            "   - Optional: `--max-frames N` to cap frames for a quicker Space run\n"
            "2. **Upload** the zip above (must contain `images/`, `labels/`, "
            "and preferably `dataset.yaml`).\n"
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


if __name__ == "__main__":
    demo = _build_ui()
    demo.launch()
