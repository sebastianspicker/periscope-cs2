"""YOLO11n-seg fine-tuning and ONNX export for cloud environments."""

from __future__ import annotations

import shutil
from collections.abc import Mapping
from contextlib import suppress
from pathlib import Path

from .constants import (
    DEFAULT_BASE_MODEL,
    DEFAULT_BATCH,
    DEFAULT_EPOCHS,
    DEFAULT_IMGSZ,
    DEFAULT_LR0,
    DEFAULT_PATIENCE,
    OUTPUT_ONNX_FP16_NAME,
    OUTPUT_ONNX_NAME,
)
from .dataset import _ensure_dataset_yaml
from .fallbacks import (
    _IMAGE_EXTENSIONS,
    _auto_train_device,
    _build_train_kwargs,
    _count_images,
    _count_labels,
    _next_batch_on_oom,
)


def _retry_batch_on_oom(batch: int, exc: BaseException) -> int | None:
    """If *exc* looks like CUDA OOM, return a halved batch (min 1); else ``None``.

    Used by :func:`train` to rebuild train kwargs and retry. Returns ``None``
    when the exception is not an OOM so callers re-raise. Prefers train_core
    when available; keeps an inline fallback for standalone vendoring.
    """
    if _next_batch_on_oom is not None:
        return _next_batch_on_oom(batch, exc)
    message = str(exc).lower()
    if "out of memory" in message or "cuda out of memory" in message:
        return max(1, int(batch) // 2)
    return None


def train(
    data_dir: str | Path,
    base_model: str = DEFAULT_BASE_MODEL,
    epochs: int = DEFAULT_EPOCHS,
    batch: int = DEFAULT_BATCH,
    imgsz: int = DEFAULT_IMGSZ,
    lr0: float = DEFAULT_LR0,
    patience: int = DEFAULT_PATIENCE,
    device: str | None = None,
    classes: Mapping[int, str] | Mapping[str, str] | None = None,
    *,
    resume: bool = False,
    verbose: bool = True,
    progress_callback: object | None = None,
    project: str | Path | None = None,
    run_name: str | None = None,
    plots: bool = True,
) -> Path:
    """Fine-tune YOLO11n-seg and export to ONNX.

    Args:
        data_dir: Path to extracted dataset (containing images/, labels/,
            optionally dataset.yaml).
        base_model: Starting checkpoint (e.g. yolo11n-seg.pt).
        epochs: Number of training epochs.
        batch: Batch size.
        imgsz: Training image size (default 416).
        lr0: Initial learning rate.
        patience: Early stopping patience.
        device: Training device. Auto-detected if None.
        classes: Optional class id→name map override for dataset.yaml /
            training. Defaults to :data:`DEFAULT_CLASSES` when yaml is missing.
        resume: If True, pass ``resume=True`` to Ultralytics so training
            continues from the last checkpoint under the run directory.
        verbose: If True, enable Ultralytics progress output (epoch logs).
        progress_callback: Optional callable ``(epoch, total_epochs) -> None``
            invoked at the end of each training epoch when Ultralytics
            callbacks are available. Ignored when the callback cannot be
            registered.
        project: Ultralytics project directory for run artifacts. Defaults to
            ``data_dir / "runs"``.
        run_name: Ultralytics run subdirectory name under ``project``.
            Defaults to ``"train"``. Re-runs use the same directory
            (``exist_ok=True``).
        plots: If True (default), enable Ultralytics plots (results.png, etc.)
            for cloud progress reporting.

    Returns:
        Path to the exported FP32 ONNX file under ``data_dir``.
        An FP16 sibling ``cs2-yolo11n-seg-fp16.onnx`` is written when
        ``onnxconverter-common`` is available (not returned separately).
        Ultralytics run artifacts (including ``results.csv`` / ``results.png``)
        are written under ``project/run_name`` (default ``data_dir/runs/train``).
    """
    from ultralytics import YOLO

    data_dir = Path(data_dir)
    images_dir = data_dir / "images"
    labels_dir = data_dir / "labels"

    if not data_dir.is_dir():
        raise FileNotFoundError(
            f"Dataset directory does not exist: {data_dir}\n"
            "  Run extract_dataset() first or pass the path returned by extract_dataset."
        )
    if not images_dir.is_dir():
        raise FileNotFoundError(
            f"Missing images directory: {images_dir}\n"
            "  Expected structure: <data_dir>/images/*.jpg|jpeg|png and <data_dir>/labels/*.txt"
        )
    if not labels_dir.is_dir():
        raise FileNotFoundError(
            f"Missing labels directory: {labels_dir}\n"
            "  Expected structure: <data_dir>/images/*.jpg|jpeg|png and <data_dir>/labels/*.txt"
        )

    n_images = _count_images(images_dir)
    n_labels = _count_labels(labels_dir)
    if n_images == 0:
        raise FileNotFoundError(
            f"No images found under {images_dir} "
            f"(looking for {sorted(_IMAGE_EXTENSIONS)} case-insensitively)."
        )
    if n_labels == 0:
        raise FileNotFoundError(f"No label .txt files found under {labels_dir}.")
    print(f"Dataset: {n_images} images, {n_labels} labels at {data_dir.resolve()}")

    yaml_path = _ensure_dataset_yaml(data_dir, classes=classes)

    # Stable Ultralytics run directory for progress reports / resume.
    if project is None:
        project = data_dir / "runs"
    project = Path(project)
    if run_name is None:
        run_name = "train"

    # Auto-detect device (prefer train_core when vendored with the package).
    if device is None:
        if _auto_train_device is not None:
            device = _auto_train_device()
        else:
            try:
                import torch

                device = "cuda:0" if torch.cuda.is_available() else "cpu"
            except ImportError:
                try:
                    from cs2_vision_access.application.ports.model_runtime import recommend_device

                    device = recommend_device()
                except ImportError:
                    device = "cpu"
    print(f"Training on device: {device}")

    # Colab / notebook-safe dataloader workers (0–2 avoids fork issues).
    workers = 2
    if device == "cpu" or str(device).startswith("cpu"):
        workers = min(workers, 2)

    use_amp = str(device).startswith("cuda")
    print(f"Loading base model: {base_model}")
    model = YOLO(base_model)

    def _on_train_epoch_end(trainer: object) -> None:
        """Print epoch progress and key metrics; optionally forward to callback."""
        epoch = int(getattr(trainer, "epoch", -1)) + 1
        total = int(getattr(trainer, "epochs", epochs) or epochs)
        metrics = getattr(trainer, "metrics", None) or {}
        loss = getattr(trainer, "loss", None)
        parts: list[str] = []
        if loss is not None:
            try:
                parts.append(f"loss={float(loss):.4f}")
            except (TypeError, ValueError):
                parts.append(f"loss={loss}")
        if isinstance(metrics, Mapping):
            for key in (
                "metrics/mAP50(B)",
                "metrics/mAP50(M)",
                "metrics/mAP50-95(B)",
                "metrics/mAP50-95(M)",
                "train/box_loss",
                "train/seg_loss",
                "val/box_loss",
                "val/seg_loss",
            ):
                if key in metrics:
                    try:
                        parts.append(f"{key.split('/')[-1]}={float(metrics[key]):.4f}")
                    except (TypeError, ValueError):
                        parts.append(f"{key.split('/')[-1]}={metrics[key]}")
        extra = f" {' '.join(parts)}" if parts else ""
        print(f"  Epoch {epoch}/{total}{extra}")
        if callable(progress_callback):
            try:
                # Prefer richer signature when the hook accepts metrics.
                progress_callback(epoch, total, metrics if isinstance(metrics, Mapping) else {})
            except TypeError:
                try:
                    progress_callback(epoch, total)
                except Exception as exc:  # noqa: BLE001 — never break training on UI hooks
                    print(f"  (progress_callback error: {exc})")
            except Exception as exc:  # noqa: BLE001 — never break training on UI hooks
                print(f"  (progress_callback error: {exc})")

    # Register epoch callback when the API is available (Ultralytics YOLO).
    # Older / alternate YOLO builds may not support add_callback.
    with suppress(Exception):
        model.add_callback("on_train_epoch_end", _on_train_epoch_end)

    print(
        f"Training: {epochs} epochs, batch={batch}, imgsz={imgsz}, "
        f"lr0={lr0}, workers={workers}, amp={use_amp}, resume={resume}, "
        f"project={project}, name={run_name}, plots={plots}"
    )

    def _assemble_train_kwargs(batch_size: int) -> dict[str, object]:
        """Build YOLO.train kwargs; prefer train_core when available."""
        if _build_train_kwargs is not None:
            return _build_train_kwargs(
                data=str(yaml_path.resolve()),
                epochs=epochs,
                imgsz=imgsz,
                batch=batch_size,
                device=str(device),
                project=str(project),
                name=str(run_name),
                exist_ok=True,
                plots=plots,
                resume=resume,
                lr0=lr0,
                patience=patience,
                workers=workers,
                amp=use_amp,
                seed=42,
                verbose=verbose,
                val=True,
            )
        # Inline fallback when train_core is not vendored with cloud.
        kwargs: dict[str, object] = dict(
            data=str(yaml_path.resolve()),
            epochs=epochs,
            batch=batch_size,
            imgsz=imgsz,
            lr0=lr0,
            patience=patience,
            device=device,
            workers=workers,
            seed=42,
            amp=use_amp,
            val=True,
            plots=plots,
            save=True,
            verbose=verbose,
            project=str(project),
            name=run_name,
            exist_ok=True,
        )
        if resume:
            kwargs["resume"] = True
        return kwargs

    current_batch = batch
    max_oom_retries = 3
    results = None
    for attempt in range(max_oom_retries + 1):
        train_kwargs = _assemble_train_kwargs(current_batch)
        try:
            results = model.train(**train_kwargs)
            break
        except Exception as exc:
            new_batch = _retry_batch_on_oom(current_batch, exc)
            if new_batch is None or attempt >= max_oom_retries:
                raise
            print(
                f"  CUDA OOM at batch={current_batch}; retrying with batch={new_batch} "
                f"(retry {attempt + 1}/{max_oom_retries})"
            )
            current_batch = new_batch
    if results is None:
        raise RuntimeError("Training failed without results (OOM retries exhausted).")

    save_dir = getattr(results, "save_dir", None)
    if save_dir is not None:
        save_dir = Path(save_dir)
        results_csv = save_dir / "results.csv"
        results_png = save_dir / "results.png"
        print(f"  Run dir: {save_dir}")
        if results_csv.is_file():
            print(f"  results.csv: {results_csv}")
        if results_png.is_file():
            print(f"  results.png: {results_png}")

    best_pt = Path(results.save_dir) / "weights" / "best.pt"
    if not best_pt.is_file():
        raise RuntimeError(
            f"Training did not produce best.pt at {best_pt}. "
            "Check Ultralytics logs, disk space, and that labels are valid YOLO-seg format."
        )

    print("Exporting FP32 ONNX...")
    model = YOLO(str(best_pt))
    model.export(format="onnx", imgsz=imgsz, simplify=True)

    onnx_path = best_pt.with_suffix(".onnx")
    if not onnx_path.is_file():
        raise RuntimeError(
            f"ONNX export failed — expected file at {onnx_path}. "
            "Ensure onnx and ultralytics export deps are installed (install_dependencies)."
        )

    final_onnx = data_dir / OUTPUT_ONNX_NAME
    shutil.copy2(str(onnx_path), str(final_onnx))
    print(f"✓ FP32 ONNX: {final_onnx} ({final_onnx.stat().st_size / 1e6:.1f} MB) — CPU inference")

    # Convert to FP16 for GPU inference (2× faster); sibling path documented for callers.
    final_fp16 = data_dir / OUTPUT_ONNX_FP16_NAME
    try:
        import onnx
        from onnxconverter_common import float16

        print("Converting to FP16 for GPU inference...")
        model_fp16 = onnx.load(str(onnx_path))
        model_fp16 = float16.convert_float_to_float16(model_fp16, keep_io_types=True)
        onnx.checker.check_model(model_fp16, full_check=True)
        onnx.save(model_fp16, str(final_fp16))
        print(
            f"✓ FP16 ONNX: {final_fp16} ({final_fp16.stat().st_size / 1e6:.1f} MB) — GPU inference"
        )
        print(f"  (train() returns FP32 path; FP16 sibling: {final_fp16.name})")
    except ImportError:
        print("(Skip FP16: onnxconverter-common not available)")
    except Exception as exc:
        print(f"(Skip FP16 conversion: {exc})")

    return final_onnx
