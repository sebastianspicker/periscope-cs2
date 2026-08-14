"""Top-level autonomous training orchestration."""

from __future__ import annotations

from collections.abc import Mapping
from pathlib import Path

from . import deps
from .bootstrap import _phase_bootstrap_labels
from .fsutil import _auto_device
from .models import (
    _DEFAULT_CS2_10K_HOLDOUT_VIDEO_FRACTION,
    AutonomousReport,
)
from .package import _phase_package_report
from .resolve import _phase_resolve_data
from .splits import _phase_held_out_split
from .train_loop import _phase_train_self_train_loop


def run_autonomous_loop(
    dataset_zip: str | Path | None = None,
    *,
    data_dir: str | Path = "cs2_data",
    iterations: int = 3,
    epochs_per_iter: int | None = None,
    batch: int | None = None,
    imgsz: int | None = None,
    lr0: float | None = None,
    patience: int | None = None,
    base_model: str | None = None,
    device: str | None = None,
    conf_threshold: float = 0.45,
    conf_low: float | None = None,
    conf_high: float | None = None,
    conf_schedule: bool = True,
    bootstrap_if_needed: bool = True,
    bootstrap_conf: float = 0.25,
    min_label_ratio: float = 0.05,
    use_edgesam: bool = False,
    download_edgesam: bool = True,
    edgesam_artifacts_dir: str | Path | None = None,
    edgesam_confidence: float = 0.4,
    edgesam_coco_fallback: bool = True,
    classes: Mapping[int, str] | Mapping[str, str] | None = None,
    origin: str = "Autonomous iterative remote train (CS2-10k capable)",
    install_deps: bool = True,
    search_roots: list[str | Path] | None = None,
    allow_leaky_val: bool = False,
    val_fraction: float = 0.2,
    profile: str = "cloud_t4",
    continue_on_self_train_error: bool = False,
    resume: bool = True,
    min_map50: float | None = None,
    min_label_growth: int = 0,
    stop_on_plateau_iters: int = 0,
    use_cs2_10k: bool = True,
    cs2_10k_maps: tuple[str, ...] = ("mirage", "dust2"),
    cs2_10k_max_shards: int = 1,
    cs2_10k_max_videos: int = 24,
    cs2_10k_frames_per_video: int = 12,
    cs2_10k_cache_dir: str | Path | None = None,
    cs2_10k_holdout_video_fraction: float = _DEFAULT_CS2_10K_HOLDOUT_VIDEO_FRACTION,
    use_teacher_gate: bool = True,
    teacher_min_iou: float = 0.3,
) -> AutonomousReport:
    """Run extract → bootstrap → held-out split → (train → self-label)×N → package.

    Parameters
    ----------
    dataset_zip:
        Zip path, or None to auto-discover via :func:`find_dataset_zip` /
        use an already-populated *data_dir* / auto-load CS2-10k.
    use_cs2_10k:
        When no zip and no local images, download a small slice of
        `RekaAI/CS2-10k <https://huggingface.co/datasets/RekaAI/CS2-10k>`_
        (WebDataset tar shards) and sample frames automatically.
    iterations:
        Number of train → self-train cycles (≥1).
    epochs_per_iter:
        Epochs each cycle (overrides profile epochs when set).
    profile:
        Named train profile from :mod:`contracts` (default ``cloud_t4``).
    bootstrap_if_needed:
        If label coverage is below *min_label_ratio*, seed labels with a
        shape prior (COCO person by default; EdgeSAM when *use_edgesam*).
    use_edgesam:
        When True, auto-label with Vombit+EdgeSAM prepare before falling
        back to COCO person (if *edgesam_coco_fallback*).
    download_edgesam:
        When *use_edgesam* and assets are missing, download teacher models
        into *edgesam_artifacts_dir* (default True).
    edgesam_artifacts_dir:
        Directory for EdgeSAM/Vombit ONNX assets. Default: existing
        ``artifacts/`` if complete, else ``data_dir/edgesam_assets``.
    edgesam_confidence:
        Detector confidence for EdgeSAM prepare (default 0.4).
    edgesam_coco_fallback:
        On EdgeSAM failure (or zero labels), fall back to COCO person
        bootstrap when *bootstrap_if_needed* (default True).
    conf_threshold:
        Self-train high-band threshold for student silhouettes (base value).
    conf_schedule:
        When True (default), raise effective conf over iterations as
        ``min(0.85, conf_threshold * (1 + 0.05 * (it - 1)))``. Applies the same
        growth to conf_high / conf_low bases. Set False for a fixed threshold.
    allow_leaky_val:
        When True, keep flat train=val layout (legacy). Default False uses a
        held-out ``images/{train,val}`` split.
    val_fraction:
        Fraction of stems reserved for val when not leaky (default 0.2).
        Ignored when CS2-10k video holdout already populated val.
    cs2_10k_holdout_video_fraction:
        When auto-loading CS2-10k, fraction of videos reserved for val
        (default 0.2). Holdout frames are promoted to ``images/val`` and are
        never randomly re-split into train. Set 0 to disable video holdout.
    continue_on_self_train_error:
        When False (default), hard self-train errors fail the run.
    use_teacher_gate:
        When True (default), multi-iter self-train passes the previous best
        (or previous-iter) ONNX + manifest as a teacher agreement gate.
        Iteration 1 skips teacher (bootstrap base is usually ``.pt``).
    teacher_min_iou:
        Minimum box IoU for student/teacher agreement (default 0.3).
    resume:
        When True (default), skip completed iterations from
        ``autonomous_state.json`` if present.
    """
    if iterations < 1:
        raise ValueError("iterations must be >= 1")
    if not 0.0 <= float(teacher_min_iou) <= 1.0:
        raise ValueError("teacher_min_iou must be in [0, 1]")
    if install_deps:
        deps.install_dependencies(gpu=True)

    device = device or _auto_device()
    data_dir = Path(data_dir)
    notes: list[str] = []
    status = "ok"
    class_map = deps.normalize_class_names(classes, default="product")

    # Resolve hyperparameters from named profile + explicit overrides.
    hp = deps.resolve_train_hyperparameters(
        profile=profile,
        epochs=epochs_per_iter,
        batch=batch,
        image_size=imgsz,
        base_model=base_model,
        lr0=lr0,
        patience=patience,
    )
    epochs = int(hp["epochs"])
    batch_size = int(hp["batch"])
    imgsz_resolved = int(hp["image_size"])
    base_model_resolved = str(hp["base_model"])
    lr0_resolved = float(hp["lr0"])
    patience_resolved = int(hp["patience"])
    notes.append(f"profile={hp['profile_name']} epochs/iter={epochs} batch={batch_size}")

    # Self-train conf bands (AL mid-band queue). Base values; may grow per-iter
    # when conf_schedule is enabled.
    conf_base = float(conf_threshold)
    conf_high_base = float(conf_high) if conf_high is not None else conf_base
    conf_low_base = float(conf_low) if conf_low is not None else conf_base * 0.7
    if conf_schedule:
        notes.append(
            "conf_schedule=True: self-train conf grows as "
            "base*(1+0.05*(it-1)) capped at 0.85 "
            f"(base conf_threshold={conf_base}, conf_high={conf_high_base}, "
            f"conf_low={conf_low_base})"
        )
    else:
        notes.append(
            f"conf_schedule=False: fixed conf_threshold={conf_base} "
            f"conf_high={conf_high_base} conf_low={conf_low_base}"
        )

    print("=== Autonomous player-shape training ===")
    print(
        f"class_map={dict(class_map)} iterations={iterations} "
        f"profile={hp['profile_name']} device={device}"
    )

    # --- Phase: resolve data ------------------------------------------------
    resolved = _phase_resolve_data(
        data_dir,
        dataset_zip=dataset_zip,
        search_roots=search_roots,
        use_cs2_10k=use_cs2_10k,
        cs2_10k_maps=cs2_10k_maps,
        cs2_10k_max_shards=cs2_10k_max_shards,
        cs2_10k_max_videos=cs2_10k_max_videos,
        cs2_10k_frames_per_video=cs2_10k_frames_per_video,
        cs2_10k_cache_dir=cs2_10k_cache_dir,
        cs2_10k_holdout_video_fraction=cs2_10k_holdout_video_fraction,
        notes=notes,
    )
    data_dir = resolved.data_dir
    images_dir = resolved.images_dir
    labels_dir = resolved.labels_dir
    n_images = resolved.n_images
    n_labels = resolved.n_labels

    # --- Phase: bootstrap labels --------------------------------------------
    bootstrap_written, n_labels = _phase_bootstrap_labels(
        data_dir,
        images_dir=images_dir,
        labels_dir=labels_dir,
        n_images=n_images,
        n_labels=n_labels,
        bootstrap_if_needed=bootstrap_if_needed,
        min_label_ratio=min_label_ratio,
        use_edgesam=use_edgesam,
        download_edgesam=download_edgesam,
        edgesam_artifacts_dir=edgesam_artifacts_dir,
        edgesam_confidence=edgesam_confidence,
        edgesam_coco_fallback=edgesam_coco_fallback,
        bootstrap_conf=bootstrap_conf,
        device=device,
        class_map=class_map,
        notes=notes,
    )

    # --- Phase: held-out split ----------------------------------------------
    split = _phase_held_out_split(
        data_dir,
        images_dir=images_dir,
        labels_dir=labels_dir,
        class_map=class_map,
        allow_leaky_val=allow_leaky_val,
        val_fraction=val_fraction,
        notes=notes,
    )

    print(
        f"Player-shape loop: {iterations} iterations × {epochs} epochs, "
        f"device={device}, images={n_images}, labels={n_labels}, "
        f"layout={split.layout.value}, class_map={dict(class_map)}"
    )

    # --- Phase: train + self-train loop -------------------------------------
    loop = _phase_train_self_train_loop(
        data_dir,
        images_dir=images_dir,
        train_images_dir=split.train_images_dir,
        train_labels_dir=split.train_labels_dir,
        layout=split.layout,
        class_map=class_map,
        iterations=iterations,
        epochs=epochs,
        batch_size=batch_size,
        imgsz_resolved=imgsz_resolved,
        base_model_resolved=base_model_resolved,
        lr0_resolved=lr0_resolved,
        patience_resolved=patience_resolved,
        device=device,
        origin=origin,
        conf_low=conf_low,
        conf_high=conf_high,
        conf_schedule=conf_schedule,
        conf_base=conf_base,
        use_teacher_gate=use_teacher_gate,
        teacher_min_iou=teacher_min_iou,
        continue_on_self_train_error=continue_on_self_train_error,
        resume=resume,
        stop_on_plateau_iters=stop_on_plateau_iters,
        notes=notes,
        status=status,
    )

    # --- Phase: package + report --------------------------------------------
    return _phase_package_report(
        data_dir,
        images_dir=images_dir,
        train_labels_dir=split.train_labels_dir,
        class_map=class_map,
        origin=origin,
        device=device,
        bootstrap_written=bootstrap_written,
        min_map50=min_map50,
        min_label_growth=min_label_growth,
        continue_on_self_train_error=continue_on_self_train_error,
        loop=loop,
        dataset_license=resolved.dataset_license,
        dataset_source=resolved.dataset_source,
        notes=notes,
    )
